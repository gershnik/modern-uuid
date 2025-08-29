// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <doctest/doctest.h>

#include <modern-uuid/uuid.h>
#include <modern-uuid/ulid.h>

#include <iostream>

#if __has_include(<unistd.h>) && __has_include(<signal.h>) && !defined(__MINGW32__) && !defined(__EMSCRIPTEN__)

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace muuid;


TEST_SUITE("fork") {

TEST_CASE("simple") {

    auto up1 = uuid::generate_time_based();
    auto up2 = uuid::generate_unix_time_based();
    auto up3 = ulid::generate();

    int pipes[2];
    REQUIRE(pipe(pipes) == 0);

    auto pid = fork();
    if (pid == 0) {
        //child
        close(pipes[0]);
        auto uc1 = uuid::generate_time_based();
        auto uc2 = uuid::generate_unix_time_based();
        auto uc3 = ulid::generate();

        while (write(pipes[1], uc1.bytes.data(), uc1.bytes.size()) == -1) {
            int err = errno;
            if (err == EINTR)
                continue;
            std::cout << "child: 1st write failed: " << err << '\n';
            exit(2);
        }
        while (write(pipes[1], uc2.bytes.data(), uc2.bytes.size()) == -1) {
            int err = errno;
            if (err == EINTR)
                continue;
            std::cout << "child: 2nd write failed: " << err << '\n';
            exit(2);
        }
        while (write(pipes[1], uc3.bytes.data(), uc3.bytes.size()) == -1) {
            int err = errno;
            if (err == EINTR)
                continue;
            std::cout << "child: 3rd write failed: " << err << '\n';
            exit(2);
        }
        close(pipes[1]);
        exit(0);
    } else {
        //parent
        close(pipes[1]);

        uint8_t buf[3 * sizeof(uuid)];
        for (size_t read_count = 0; read_count < sizeof(buf); ) {
            auto res = read(pipes[0], buf + read_count, sizeof(buf) - read_count);
            if (res < 0) {
                int err = errno;
                if (err == EINTR)
                    continue;
                FAIL_CHECK("pipe read failed, errno", err);
                break;
            }
            read_count += res;
        }
        close(pipes[0]);

        int stat = 0;
        for ( ; ; ) {
            pid_t res = waitpid(pid, &stat, 0);
            if (res == -1) {
                int err = errno;
                if (err == EINTR)
                    continue;
                FAIL("waitpid failed with errno: ", err);
            }
            break;
        }
        REQUIRE(bool(WIFEXITED(stat)));
        REQUIRE(WEXITSTATUS(stat) == 0);

        uuid ur1(std::span<uint8_t, sizeof(uuid)>{buf, sizeof(uuid)});
        uuid ur2(std::span<uint8_t, sizeof(uuid)>{buf + sizeof(uuid), sizeof(uuid)});
        ulid ur3(std::span<uint8_t, sizeof(ulid)>{buf + 2 * sizeof(uuid), sizeof(ulid)});

        std::cout << "child v1: " << ur1 << '\n';
        std::cout << "child v7: " << ur2 << '\n';
        std::cout << "child ulid: " << ur3 << '\n';

        CHECK(ur1.get_variant() == uuid::variant::standard);
        CHECK(ur1.get_type() == uuid::type::time_based);
        CHECK(ur2.get_variant() == uuid::variant::standard);
        CHECK(ur2.get_type() == uuid::type::unix_time_based);

        CHECK(up1 != ur1);
        CHECK(up2 < ur2);
        CHECK(up3 != ur3);
    }

}

}

#endif

