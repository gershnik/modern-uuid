// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <doctest/doctest.h>

#include <modern-uuid/uuid.h>

#include <iostream>

#if __has_include(<unistd.h>) && __has_include(<signal.h>) && !defined(__MINGW32__)

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace muuid;


TEST_SUITE("fork") {

TEST_CASE("simple") {

    auto u1 = uuid::generate_time_based();
    auto u2 = uuid::generate_unix_time_based();

    int pipes[2];
    REQUIRE(pipe(pipes) == 0);

    auto pid = fork();
    if (pid == 0) {
        //child
        close(pipes[0]);
        auto u3 = uuid::generate_time_based();
        auto u4 = uuid::generate_unix_time_based();

        while (write(pipes[1], u3.bytes().data(), u3.bytes().size()) == -1) {
            int err = errno;
            if (err == EINTR)
                continue;
            std::cout << "child: 1st write failed: " << err << '\n';
            exit(2);
        }
        while (write(pipes[1], u4.bytes().data(), u4.bytes().size()) == -1) {
            int err = errno;
            if (err == EINTR)
                continue;
            std::cout << "child: 2nd write failed: " << err << '\n';
            exit(2);
        }
        close(pipes[1]);
        exit(0);
    } else {
        //parent
        close(pipes[1]);

        uint8_t buf[2 * sizeof(uuid)];
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

        uuid u3(std::span<uint8_t, sizeof(uuid)>{buf, sizeof(uuid)});
        uuid u4(std::span<uint8_t, sizeof(uuid)>{buf + sizeof(uuid), sizeof(uuid)});

        std::cout << "child v1: " << u3 << '\n';
        std::cout << "child v7: " << u4 << '\n';

        CHECK(u3.get_variant() == uuid::variant::standard);
        CHECK(u3.get_type() == uuid::type::time_based);
        CHECK(u4.get_variant() == uuid::variant::standard);
        CHECK(u4.get_type() == uuid::type::unix_time_based);

        CHECK(u1 != u3);
        CHECK(u2 < u4);
    }

}

}

#endif

