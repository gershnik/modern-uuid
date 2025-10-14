// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#define BIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD

#include <modern-uuid/uuid.h>

#include "node_id.h"
#include "random_generator.h"
#include "threading.h"
#include "fork_handler.h"

#include <array>
#include <mutex>

#if __has_include(<net/if.h>)
    #define HAVE_NET_IF_H 1
    #include <net/if.h>

    #if __has_include(<net/if_dl.h>)
        #define HAVE_NET_IF_DL_H 1
        #include <net/if_dl.h>
    #endif

    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #if __has_include(<sys/sockio.h>)
        #include <sys/sockio.h>
    #endif
    #include <netinet/in.h>
    #include <unistd.h>

#elif defined(_WIN32) || defined(_WIN64)
    #define WIN32_LEAN_AND_MEAN

    #include <Windows.h>
    #include <WinSock2.h>
    #include <iphlpapi.h>

    #ifndef __MINGW32__
        #pragma comment(lib, "iphlpapi.lib")
    #endif
#endif

#include <cstring>
#include <type_traits>
#include <algorithm>
#include <vector>

using namespace muuid;

#ifdef HAVE_NET_IF_H

    #define MUUID_DECLARE_MEMBER_DETECTOR(type, member, name) \
        struct name##_detector { \
            template<class T> \
            static std::true_type detect(decltype(T::member) *); \
            template<class T> \
            static std::false_type detect(...); \
        }; \
        constexpr bool name = decltype(name##_detector::detect<type>(nullptr))::value


    MUUID_DECLARE_MEMBER_DETECTOR(struct sockaddr, sa_len, sockaddr_has_sa_len);


    MUUID_DECLARE_MEMBER_DETECTOR(struct ifreq, ifr_hwaddr, ifreq_has_ifr_hwaddr);


    /*
     * BSD 4.4 defines the size of an ifreq to be
     * max(sizeof(ifreq), sizeof(ifreq.ifr_name)+ifreq.ifr_addr.sa_len
     * However, on some systems, sa_len isn't present, so the size is
     * just sizeof(struct ifreq)
     */
    template<std::same_as<struct ifreq> T>
    static inline size_t ifreq_size(const T & req) {
        if constexpr (sockaddr_has_sa_len) {
            return std::max(sizeof(struct ifreq), 
                            sizeof(req.ifr_name) + req.ifr_addr.sa_len);
        } else {
            return sizeof(struct ifreq);
        }
    }


    template<std::same_as<struct ifreq> T>
    static inline const sockaddr & ifreq_hwaddr(const T & req) {
        if constexpr (ifreq_has_ifr_hwaddr)
            return req.ifr_hwaddr;
        else
            return req.ifr_addr;
    }

    template<class R, class FD, class T>
    auto ioctl_type_helper(R (*)(FD, T, ...)) {
        return T{};
    }

    template<class R, class FD, class T, class... Rest>
    auto ioctl_type_helper(R (*)(FD, T, Rest...)) {
        return T{};
    }

    using ioctl_type = decltype(ioctl_type_helper(ioctl));
#endif

static auto get_hardware_node_id(std::span<uint8_t, 6> dest) -> bool {

#if defined(HAVE_NET_IF_H) && (defined(SIOCGIFHWADDR) || defined(SIOCGENADDR) || defined(HAVE_NET_IF_DL_H))
#ifdef __HAIKU__
    int sock_af = AF_LINK;
#else
    int sock_af = AF_INET;
#endif 
    auto sd = socket(sock_af, SOCK_DGRAM, IPPROTO_IP);
    if (sd < 0)
        return false;

    struct autoclose_sd_t {
        decltype(sd) s;
        ~autoclose_sd_t() { close(s); }
    } autoclose_sd{sd};
    
    std::vector<char> buf(1024);

    struct ifconf ifc;
    for ( ; ; ) {
        ifc.ifc_len = int(buf.size());
        ifc.ifc_buf = buf.data();
        if (ioctl(sd, static_cast<ioctl_type>(SIOCGIFCONF), &ifc) < 0)
            return false;
        if (int(buf.size()) - ifc.ifc_len > int(sizeof(struct ifreq)))
            break;
        buf.resize(buf.size() + 1024);
    }
    
    if (ioctl(sd, static_cast<ioctl_type>(SIOCGIFCONF), &ifc) < 0)
        return false;
    
    struct ifreq * ifrp;
    for (size_t i = 0; i < size_t(ifc.ifc_len); i+= ifreq_size(*ifrp)) {
        ifrp = (struct ifreq *)((uint8_t *)ifc.ifc_buf + i);

        struct ifreq ifr;
        strncpy(ifr.ifr_name, ifrp->ifr_name, IFNAMSIZ);

        uint8_t * res = nullptr;
#if defined(SIOCGIFHWADDR)
        if (ioctl(sd, static_cast<ioctl_type>(SIOCGIFHWADDR), &ifr) < 0)
            continue;
        res = (uint8_t *)&ifreq_hwaddr(ifr).sa_data;
#elif defined(SIOCGENADDR)
        if (ioctl(sd, static_cast<ioctl_type>(SIOCGENADDR), &ifr) < 0)
            continue;
        res = (uint8_t *) ifr.ifr_enaddr;
#elif defined(HAVE_NET_IF_DL_H)
        auto sdlp = (struct sockaddr_dl *) &ifrp->ifr_addr;
        if ((sdlp->sdl_family != AF_LINK) || (sdlp->sdl_alen != 6))
            continue;
        res = (uint8_t *)LLADDR(sdlp);
#else
        static_assert(false);
#endif 
        if (res == nullptr || (!res[0] && !res[1] && !res[2] && !res[3] && !res[4] && !res[5]))
            continue;
        
        memcpy(dest.data(), res, 6);
        return true;
    }

#elif defined(_WIN32) || defined(_WIN64)

    static_assert(MAX_ADAPTER_ADDRESS_LENGTH >= 6);

    ULONG addresses_size = sizeof(IP_ADAPTER_ADDRESSES);
    auto addresses = (IP_ADAPTER_ADDRESSES *)malloc(addresses_size);
    if (!addresses)
        return false;
    struct autofree_addresses_t {
        decltype(addresses) & addresses;
        ~autofree_addresses_t() { free(addresses); }
    } autofree_addresses{addresses};

    for ( ; ; ) {
        
        ULONG err = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, addresses, &addresses_size);
        if (err == ERROR_BUFFER_OVERFLOW) {
            free(addresses);
            addresses = (IP_ADAPTER_ADDRESSES *)malloc(addresses_size);
            if (!addresses)
                return false;
            continue;
        }
        if (err != 0)
            return false;
        break;
    }

    for (auto cur = addresses; cur; cur = cur->Next) {
        uint8_t * res = cur->PhysicalAddress + (cur->PhysicalAddressLength - 6);
        if (!res[0] && !res[1] && !res[2] && !res[3] && !res[4] && !res[5])
            continue;
        memcpy(dest.data(), res, 6);
        return true;
    }

#endif


    return false;
}


std::array<uint8_t, 6> g_node_id;
bool g_node_id_set = false;

#if MUUID_MULTITHREADED

static std::mutex & get_node_id_mutex() {
    return impl::reset_on_fork_singleton<std::mutex>::instance();
}

#else

static impl::null_mutex & get_node_id_mutex() {
    static impl::null_mutex ret;
    return ret;
}

#endif

static std::array<uint8_t, 6> generate_node_id(node_id type) {

    std::array<uint8_t, 6> ret;

    if (type == node_id::detect_system) {
        if (get_hardware_node_id(ret))
            return ret;
    }

    auto & gen = muuid::impl::get_random_generator();
    std::uniform_int_distribution<unsigned> distrib(0, 255);

    for(auto & b: ret)
        b = uint8_t(distrib(gen));

    // Set multicast bit, to prevent conflicts
    // with IEEE 802 addresses obtained from
    // network cards
    ret[0] |= 0x01;

    return ret;
}


auto muuid::set_node_id(node_id type) -> std::array<uint8_t, 6> {
    std::lock_guard guard{get_node_id_mutex()};

    g_node_id = generate_node_id(type);
    g_node_id_set = true;
    return g_node_id;
}
    
void muuid::set_node_id(std::span<const uint8_t, 6> id) {
    std::lock_guard guard{get_node_id_mutex()};

    memcpy(&g_node_id, id.data(), id.size());
    g_node_id_set = true;
}

std::array<uint8_t, 6> muuid::impl::get_node_id() {

    std::lock_guard guard{get_node_id_mutex()};

    if (!g_node_id_set) {
        g_node_id = generate_node_id(node_id::detect_system);
        g_node_id_set = true;
    }

    return g_node_id;
}


