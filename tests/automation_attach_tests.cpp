// The attach endpoint and its protocol (MCP 1.0.5, attach_endpoint.hpp):
// - the hello and answer lines round trip, and control bytes in a client's
//   self-reported name cannot break a line;
// - the peer rule refuses another user (attach_peer_allowed: a test cannot
//   switch users, so the rule the platform files apply to every peer's
//   kernel-reported user is checked directly, and the endpoint's own
//   directory or DACL is checked below);
// - a same-user connection is accepted, carries bytes both ways, and is
//   listed while the endpoint lives; on POSIX the socket is 0600 in a 0700
//   directory of the user's own;
// - interrupt ends a waiting accept;
// - a stalled greeting is aborted (abort_attach_connection).

#include "src/runtime/automation/attach_endpoint.hpp"

#include <stxlib/clocks.hpp>
#include <stxlib/pipes.hpp>
#include <stxlib/result.hpp>
#include <stxlib/threads.hpp>
#include <stxlib/types.hpp>
#include <stxlib/view.hpp>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace gui_forms::automation;

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "automation attach test failed: " << message << '\n';
    std::exit(1);
}

void protocol_lines() {
    AttachHello hello{};
    hello.client_name = "claude-code\tinjected\nok";
    hello.client_version = "2.1";
    const std::string line = attach_hello_line(hello);
    require(line.back() == '\n' && line.find('\n') == line.size() - 1U, "a hello is one line");
    AttachHello read{};
    require(parse_attach_hello(std::string_view(line).substr(0U, line.size() - 1U), read), "a hello parses");
    require(read.client_name == "claude-code injected ok" && read.client_version == "2.1",
            "control bytes in a self-reported name become spaces");
    AttachHello bad{};
    require(!parse_attach_hello("{\"jsonrpc\":\"2.0\"}", bad), "MCP's first message is not a hello");
    AttachAnswer refused{};
    refused.reason = "the user did not allow the connection";
    const std::string refusal = attach_answer_line(refused);
    AttachAnswer parsed{};
    require(parse_attach_answer(std::string_view(refusal).substr(0U, refusal.size() - 1U), parsed) &&
                !parsed.accepted && parsed.reason == refused.reason,
            "a refusal carries its reason");
    AttachAnswer accepted{};
    accepted.accepted = true;
    require(attach_answer_line(accepted) == "ok\n", "an acceptance is ok");
    std::string long_name(4000U, 'n');
    hello.client_name = long_name;
    require(attach_hello_line(hello).size() < maximum_attach_line, "a long name is cut to fit a line");
}

void peer_rule() {
    require(attach_peer_allowed(501U, 501U), "the same user may connect");
    require(!attach_peer_allowed(501U, 502U), "another user is refused");
    require(!attach_peer_allowed(0U, 501U) && !attach_peer_allowed(501U, 0U),
            "root and a user are different users either way");
}

struct AcceptRun final {
    AttachEndpoint* endpoint{};
    AttachStreams streams{};
    AttachWait waited{AttachWait::failed};
    std::string refused{};
};

void accept_thread(const stxlib::StopSignal&, void* argument) noexcept {
    AcceptRun& run = *static_cast<AcceptRun*>(argument);
    run.waited = (*run.endpoint).accept(run.streams, run.refused);
}

bool write_line(stxlib::PipeWriter& output, std::string_view text) {
    return write_attach_line(output, text);
}

void same_user_connection() {
    std::string error{};
    const std::unique_ptr<AttachEndpoint> endpoint = AttachEndpoint::listen("app.tests.attach", error);
    require(endpoint != nullptr, "the endpoint listens");
#if !defined(_WIN32)
    struct stat socket_status {};
    require(lstat((*endpoint).address().c_str(), &socket_status) == 0 && S_ISSOCK(socket_status.st_mode) &&
                (socket_status.st_mode & 0777U) == 0600U && socket_status.st_uid == getuid(),
            "the socket is the user's own, mode 0600");
    const std::string directory = (*endpoint).address().substr(0U, (*endpoint).address().rfind('/'));
    struct stat directory_status {};
    require(lstat(directory.c_str(), &directory_status) == 0 && S_ISDIR(directory_status.st_mode) &&
                (directory_status.st_mode & 0077U) == 0U && directory_status.st_uid == getuid(),
            "the socket's directory is the user's own, mode 0700");
#endif
    bool listed = false;
    for (const AttachListing& listing : list_attach_endpoints()) {
        if (listing.address == (*endpoint).address()) {
            listed = listing.identity == "app.tests.attach";
        }
    }
    require(listed, "the endpoint is listed under its identity");

    AcceptRun run{};
    run.endpoint = endpoint.get();
    stxlib::OperationResult<stxlib::Thread> started = stxlib::Thread::start(&accept_thread, &run);
    require(started.succeeded(), "the accept thread starts");
    stxlib::Thread thread = std::move(*started.value());
    AttachStreams client{};
    require(connect_attach_endpoint((*endpoint).address(), client, error), "a client of the same user connects");
    static_cast<void>(thread.join());
    require(run.waited == AttachWait::connected, "the same user's connection is accepted");

    AttachHello hello{};
    hello.client_name = "attach test";
    require(write_line(client.output, attach_hello_line(hello)), "the client says hello");
    std::string line{};
    require(read_attach_line(run.streams.input, line), "the program reads the hello");
    AttachHello read{};
    require(parse_attach_hello(line, read) && read.client_name == "attach test", "the hello names the client");
    AttachAnswer accepted{};
    accepted.accepted = true;
    require(write_line(run.streams.output, attach_answer_line(accepted)), "the program answers");
    require(read_attach_line(client.input, line) && line == "ok", "the client reads ok");
}

void interrupt_ends_accept() {
    std::string error{};
    const std::unique_ptr<AttachEndpoint> endpoint = AttachEndpoint::listen("app.tests.interrupt", error);
    require(endpoint != nullptr, "a second endpoint listens");
    AcceptRun run{};
    run.endpoint = endpoint.get();
    stxlib::OperationResult<stxlib::Thread> started = stxlib::Thread::start(&accept_thread, &run);
    require(started.succeeded(), "the accept thread starts");
    stxlib::Thread thread = std::move(*started.value());
    (*endpoint).interrupt();
    static_cast<void>(thread.join());
    require(run.waited == AttachWait::interrupted, "interrupt ends the wait");
}

struct ReadRun final {
    stxlib::PipeReader* input{};
    bool read{true};
};

void read_thread(const stxlib::StopSignal&, void* argument) noexcept {
    ReadRun& run = *static_cast<ReadRun*>(argument);
    std::string line{};
    run.read = read_attach_line(*run.input, line);
}

void stalled_greeting_is_aborted() {
    std::string error{};
    const std::unique_ptr<AttachEndpoint> endpoint = AttachEndpoint::listen("app.tests.stall", error);
    require(endpoint != nullptr, "a third endpoint listens");
    AcceptRun run{};
    run.endpoint = endpoint.get();
    stxlib::OperationResult<stxlib::Thread> started = stxlib::Thread::start(&accept_thread, &run);
    require(started.succeeded(), "the accept thread starts");
    stxlib::Thread thread = std::move(*started.value());
    AttachStreams client{};
    require(connect_attach_endpoint((*endpoint).address(), client, error), "a client connects and says nothing");
    static_cast<void>(thread.join());
    require(run.waited == AttachWait::connected, "the silent client is accepted");
    ReadRun reading{};
    reading.input = &run.streams.input;
    stxlib::OperationResult<stxlib::Thread> reader = stxlib::Thread::start(&read_thread, &reading);
    require(reader.succeeded(), "the greeting reader starts");
    stxlib::Thread reader_thread = std::move(*reader.value());
    stxlib::sleep_for(stxlib::Duration::milliseconds(50));
    abort_attach_connection(static_cast<std::int64_t>(run.streams.input.native_handle()));
    static_cast<void>(reader_thread.join());
    require(!reading.read, "an aborted greeting reads no line");
}

} // namespace

int main() {
    protocol_lines();
    peer_rule();
    same_user_connection();
    interrupt_ends_accept();
    stalled_greeting_is_aborted();
    std::cout << "automation attach tests passed\n";
    return 0;
}
