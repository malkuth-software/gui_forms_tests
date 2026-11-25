// The attach endpoint and its protocol (MCP 1.0.5, attach_endpoint.hpp; the
// owner's attach design, revision 3):
// - a connection's first line: a JSON object is MCP (its method, id,
//   self-reported client read), anything else the console;
// - the console's commands, a call's arguments as one JSON object or as
//   key=value items;
// - a refused initialize's JSON-RPC error line, the discovery entry's text,
//   with the access line;
// - the peer rule refuses another user (attach_peer_allowed: a test cannot
//   switch users, so the rule the platform files apply to every peer's
//   kernel-reported user is checked directly, and the endpoint's own
//   directory and socket modes are checked below);
// - a same-user connection is accepted; its first line can be peeked and is
//   still read after; on POSIX the socket is 0600 in a 0700 directory of
//   the user's own;
// - the discovery entry appears with the endpoint, follows publish, and
//   goes with it; an ended process's entry is removed by the next listing;
// - a timed accept ends, interrupt ends a waiting accept, and a stalled
//   connection is aborted (abort_attach_connection);
// - the command folder (CommandDoor, MCP round 5): its line framing and
//   request IDs, and on POSIX a round trip through its FIFOs, their modes,
//   and the folder's removal.

#include "src/runtime/automation/attach_endpoint.hpp"

#include <stxlib/clocks.hpp>
#include <stxlib/pipes.hpp>
#include <stxlib/process.hpp>
#include <stxlib/result.hpp>
#include <stxlib/threads.hpp>
#include <stxlib/types.hpp>
#include <stxlib/view.hpp>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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

void require(bool condition, const std::string& message) {
    require(condition, message.c_str());
}

bool contains(std::string_view text, std::string_view part) {
    return text.find(part) != std::string_view::npos;
}

void opening_lines() {
    const AttachOpening initialize = read_attach_opening(
        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\","
        "\"clientInfo\":{\"name\":\"claude-code\\tinjected\",\"version\":\"2.1\"},\"capabilities\":{}}}");
    require(initialize.json && initialize.method == "initialize" && initialize.id_json == "7",
            "an initialize is MCP, with its method and id");
    require(initialize.client_name == "claude-code injected" && initialize.client_version == "2.1",
            "the client is read as it named itself, control bytes as spaces");
    const AttachOpening resumed = read_attach_opening(
        "  {\"jsonrpc\":\"2.0\",\"id\":\"a\",\"method\":\"initialize\",\"params\":{\"resume\":\"0123abcd\"}}\r");
    require(resumed.json && resumed.id_json == "\"a\"",
            "1.0.5's params.resume is harmless (there are no sessions); a string id stays a string");
    require(!read_attach_opening("help").json && !read_attach_opening("").json &&
                !read_attach_opening("call windows {}").json,
            "a line that is not a JSON object is the console");
    const AttachOpening broken = read_attach_opening("{not json");
    require(broken.json && broken.method.empty() && broken.id_json == "null",
            "a line that opens an object is MCP's even unparsed (MCP's server answers it)");
}

void console_commands() {
    require(parse_console_command("").verb == ConsoleVerb::none, "a blank line is nothing");
    require(parse_console_command(" help ").verb == ConsoleVerb::help, "help");
    require(parse_console_command("tools").verb == ConsoleVerb::tools, "tools");
    require(parse_console_command("transcript").verb == ConsoleVerb::transcript, "transcript");
    require(parse_console_command("quit").verb == ConsoleVerb::quit, "quit");
    require(parse_console_command("resume 0123abcd").verb == ConsoleVerb::resume &&
                parse_console_command("resume").verb == ConsoleVerb::resume,
            "1.0.5's resume is kept, harmless, its token ignored");
    const ConsoleCommand dotted = parse_console_command("help plan_paint.draw");
    require(dotted.verb == ConsoleVerb::help && dotted.topic == "plan_paint.draw", "help takes a dotted path");
    const ConsoleCommand bare = parse_console_command("call windows");
    require(bare.verb == ConsoleVerb::call && bare.tool == "windows" && bare.arguments_json == "{}",
            "a call without arguments has an empty object");
    const ConsoleCommand object = parse_console_command("call act {\"action\": \"press\", \"target\": \"ok\"}");
    require(object.verb == ConsoleVerb::call && object.tool == "act" &&
                object.arguments_json == "{\"action\":\"press\",\"target\":\"ok\"}",
            "a call's JSON object is taken as it is");
    const ConsoleCommand items = parse_console_command(
        "call act action=press target=auto.ok count=2 wait=true text=\"two words\" points=[[1,2],[3,4]]");
    require(items.verb == ConsoleVerb::call &&
                items.arguments_json == "{\"action\":\"press\",\"target\":\"auto.ok\",\"count\":2,\"wait\":true,"
                                        "\"text\":\"two words\",\"points\":[[1,2],[3,4]]}",
            "key=value items make one object: JSON values as JSON, the rest as strings");
    require(parse_console_command("call act loose").verb == ConsoleVerb::unknown,
            "an item without '=' says what it expected");
    require(parse_console_command("call act {\"open\": 1").verb == ConsoleVerb::unknown,
            "an unclosed object is refused");
    require(parse_console_command("call act text=\"open").verb == ConsoleVerb::unknown,
            "an unclosed quote is refused");
    require(parse_console_command("call").verb == ConsoleVerb::unknown, "a call needs a tool");
    const ConsoleCommand other = parse_console_command("ls -l");
    require(other.verb == ConsoleVerb::unknown && contains(other.error, "help, tools, call"),
            "anything else names the commands");
}

void error_lines() {
    const std::string line = json_rpc_error_line("7", -32001, "the user did not \"allow\" it\nat all");
    require(line == "{\"jsonrpc\":\"2.0\",\"id\":7,\"error\":{\"code\":-32001,\"message\":\"the user did not "
                    "\\\"allow\\\" it at all\"}}\n",
            "a refusal is one JSON-RPC error line on the request's id");
    require(contains(json_rpc_error_line("\"a\"", -1, "x"), "\"id\":\"a\""), "a string id is kept");
    require(contains(json_rpc_error_line("not json", -1, "x"), "\"id\":null"), "an id that does not parse is null");
}

void info_entries() {
    AttachListing written{};
    written.identity = "app.tests.attach";
    written.name = "Attach\nTest";
    written.process = 4242;
    written.executable = "/opt/attach test/bin/app";
    written.address = "/run/gui-forms/4242.sock";
    written.tools = 23U;
    written.note = "Plan Paint: draws plans.";
    written.access = "This program takes commands from your own user account only.";
    const std::string text = attach_info_text(written);
    AttachListing read{};
    require(parse_attach_info(text, read), "an entry reads back");
    require(read.identity == written.identity && read.name == "Attach Test" && read.process == 4242 &&
                read.executable == written.executable && read.address == written.address && read.tools == 23U &&
                read.note == written.note && read.access == written.access,
            "every field survives, a line break in the name as a space");
    AttachListing incomplete{};
    require(!parse_attach_info("name: x\nsocket: /s\n", incomplete), "an entry without a pid is refused");
    require(!parse_attach_info("pid: 12x\nsocket: /s\n", incomplete), "a pid that is not a number is refused");
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
    std::int64_t limit{-1};
};

void accept_thread(const stxlib::StopSignal&, void* argument) noexcept {
    AcceptRun& run = *static_cast<AcceptRun*>(argument);
    run.waited = (*run.endpoint).accept(run.streams, run.refused, run.limit);
}

AttachProgram test_program(const char* identity) {
    AttachProgram program{};
    program.identity = identity;
    program.name = "Attach Test";
    program.executable = "/tests/attach";
    return program;
}

// The entry the listing has for `address`, or one with process 0.
AttachListing listed(const std::string& address) {
    for (const AttachListing& listing : list_attach_endpoints()) {
        if (listing.address == address) return listing;
    }
    return AttachListing{};
}

bool file_exists(const std::string& path) {
    const std::ifstream file(path);
    return file.good();
}

std::string info_path(const std::string& socket) {
    return socket.substr(0U, socket.size() - std::string_view(".sock").size()) + ".info";
}

void same_user_connection() {
    std::string error{};
    std::string address{};
    {
        const std::unique_ptr<AttachEndpoint> endpoint =
            AttachEndpoint::listen(test_program("app.tests.attach"), error);
        require(endpoint != nullptr, "the endpoint listens");
        address = (*endpoint).address();
        std::string path_error{};
        require(attach_socket_path(static_cast<std::int64_t>(stxlib::process_id()), path_error) == address,
                "attach_socket_path names the socket");
#if !defined(_WIN32)
        struct stat socket_status {};
        require(lstat(address.c_str(), &socket_status) == 0 && S_ISSOCK(socket_status.st_mode) &&
                    (socket_status.st_mode & 0777U) == 0600U && socket_status.st_uid == getuid(),
                "the socket is the user's own, mode 0600");
        const std::string directory = address.substr(0U, address.rfind('/'));
        struct stat directory_status {};
        require(lstat(directory.c_str(), &directory_status) == 0 && S_ISDIR(directory_status.st_mode) &&
                    (directory_status.st_mode & 0077U) == 0U && directory_status.st_uid == getuid(),
                "the run directory is the user's own, mode 0700");
#endif
        const AttachListing first = listed(address);
        require(first.identity == "app.tests.attach" && first.name == "Attach Test" &&
                    first.executable == "/tests/attach" && first.tools == 0U,
                "the discovery entry appears with the endpoint");
        (*endpoint).publish(12U, "The test program.\nMore of its note.");
        const AttachListing published = listed(address);
        require(published.tools == 12U && published.note == "The test program.",
                "publish rewrites the entry: the tool count and the note's first line");

        AcceptRun run{};
        run.endpoint = endpoint.get();
        stxlib::OperationResult<stxlib::Thread> started = stxlib::Thread::start(&accept_thread, &run);
        require(started.succeeded(), "the accept thread starts");
        stxlib::Thread thread = std::move(*started.value());
        AttachStreams client{};
        require(connect_attach_endpoint(address, client, error), "a client of the same user connects");
        static_cast<void>(thread.join());
        require(run.waited == AttachWait::connected, "the same user's connection is accepted");
#if defined(__APPLE__) || defined(__linux__)
        require(run.streams.peer_process == static_cast<std::int64_t>(stxlib::process_id()),
                "the kernel names the connecting process, for the console's label");
#endif

        require(write_attach_line(client.output, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}\nhelp\n"),
                "the client writes two lines");
        std::string line{};
        require(peek_attach_line(run.streams, line, 5000) &&
                    line == "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}",
                "the first line is peeked");
        require(read_attach_line(run.streams.input, line) &&
                    line == "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}",
                "a peeked line is still read: MCP's server sees it");
        require(read_attach_line(run.streams.input, line) && line == "help", "and the line after it");
        require(write_attach_line(run.streams.output, "ok\n"), "the program answers");
        require(read_attach_line(client.input, line) && line == "ok", "the client reads the answer");
    }
    require(listed(address).process == 0 && !file_exists(info_path(address)),
            "the discovery entry goes with the endpoint");
}

void ended_entries_are_removed() {
    std::string error{};
    const std::string directory = attach_run_directory(true, error);
    require(!directory.empty(), "the run directory can be made");
#if defined(_WIN32)
    const std::string stale = directory + "\\2147483646.info";
#else
    const std::string stale = directory + "/2147483646.info";
#endif
    {
        std::ofstream file(stale);
        file << "name: gone\npid: 2147483646\nsocket: " << directory << "/2147483646.sock\n";
    }
    require(file_exists(stale), "an ended process's entry is written");
    bool shown = false;
    for (const AttachListing& listing : list_attach_endpoints()) shown = shown || listing.process == 2147483646;
    require(!shown && !file_exists(stale), "the listing leaves out and removes an ended process's entry");
}

void timed_accept_ends() {
    std::string error{};
    const std::unique_ptr<AttachEndpoint> endpoint = AttachEndpoint::listen(test_program("app.tests.timed"), error);
    require(endpoint != nullptr, "a second endpoint listens");
    AttachStreams streams{};
    std::string refused{};
    require((*endpoint).accept(streams, refused, 50) == AttachWait::timed_out, "a timed accept ends without a peer");
}

void interrupt_ends_accept() {
    std::string error{};
    const std::unique_ptr<AttachEndpoint> endpoint =
        AttachEndpoint::listen(test_program("app.tests.interrupt"), error);
    require(endpoint != nullptr, "a third endpoint listens");
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

void stalled_connection_is_aborted() {
    std::string error{};
    const std::unique_ptr<AttachEndpoint> endpoint = AttachEndpoint::listen(test_program("app.tests.stall"), error);
    require(endpoint != nullptr, "a fourth endpoint listens");
    AcceptRun run{};
    run.endpoint = endpoint.get();
    stxlib::OperationResult<stxlib::Thread> started = stxlib::Thread::start(&accept_thread, &run);
    require(started.succeeded(), "the accept thread starts");
    stxlib::Thread thread = std::move(*started.value());
    AttachStreams client{};
    require(connect_attach_endpoint((*endpoint).address(), client, error), "a client connects and says nothing");
    static_cast<void>(thread.join());
    require(run.waited == AttachWait::connected, "the silent client is accepted");
    std::string line{};
    require(!peek_attach_line(run.streams, line, 50), "a peek gives up on a silent client");
    ReadRun reading{};
    reading.input = &run.streams.input;
    stxlib::OperationResult<stxlib::Thread> reader = stxlib::Thread::start(&read_thread, &reading);
    require(reader.succeeded(), "the reader starts");
    stxlib::Thread reader_thread = std::move(*reader.value());
    stxlib::sleep_for(stxlib::Duration::milliseconds(50));
    abort_attach_connection(static_cast<std::int64_t>(run.streams.input.native_handle()));
    static_cast<void>(reader_thread.join());
    require(!reading.read, "an aborted connection reads no line");
}

} // namespace

void door_lines() {
    const DoorLine plain = read_door_line("  help plan_paint.draw \r");
    require(plain.request.empty() && plain.command == "help plan_paint.draw", "a line without an ID is the command");
    const DoorLine tagged = read_door_line("#a-7 plan_paint.draw shape=line");
    require(tagged.request == "a-7" && tagged.command == "plan_paint.draw shape=line", "#<id> leads the command");
    require(read_door_line("#?x help").request.empty(), "an ID of other characters is no ID");
    require(read_door_line("#12345678901234567 help").request.empty(), "an ID past 16 characters is no ID");
    require(door_reply_frame("ok: drew\n.hidden", "7", false) == "ok: drew\n..hidden\n. #7 ok\n",
            "a reply's lines, a dot doubled, then the end line with the ID");
    require(door_reply_frame("", "", true) == ". error\n", "an empty failure is its end line");
    require(door_id_valid("3fa9c21b") && !door_id_valid("3FA9C21B") && !door_id_valid("3fa9c21") &&
                !door_id_valid("../../x1"),
            "an ID is 8 lowercase hexadecimal digits");
    AttachListing written{};
    written.process = 7;
    written.address = "/s";
    written.door = "3fa9c21b";
    AttachListing read{};
    require(parse_attach_info(attach_info_text(written), read) && read.door == "3fa9c21b",
            "the discovery entry names the folder");
    written.door = "../home";
    require(parse_attach_info(attach_info_text(written), read) && read.door.empty(), "a bad ID is not read");
}

#if !defined(_WIN32)
void door_round_trip() {
    std::string error{};
    std::string folder{};
    {
        const std::unique_ptr<CommandDoor> door = CommandDoor::open(error);
        require(door != nullptr, "the command folder opens: " + error);
        folder = (*door).folder();
        require(folder == "/tmp/" + (*door).id() && door_id_valid((*door).id()), "the folder is /tmp/<id>");
        struct stat status {};
        require(lstat(folder.c_str(), &status) == 0 && S_ISDIR(status.st_mode) &&
                    (status.st_mode & 0777U) == 0700U && status.st_uid == getuid(),
                "the folder is the user's own, mode 0700");
        for (const char* const name : {"/in", "/out"}) {
            struct stat fifo {};
            require(lstat((folder + name).c_str(), &fifo) == 0 && S_ISFIFO(fifo.st_mode) &&
                        (fifo.st_mode & 0777U) == 0600U,
                    "its pipes are FIFOs, mode 0600");
        }
        require((*door).send_line("help") == "echo 'help' > " + folder + "/in; cat " + folder + "/out",
                "the printed command");
        const int writer = open((folder + "/in").c_str(), O_WRONLY | O_NONBLOCK);
        require(writer >= 0, "a writer opens `in` at once");
        const std::string_view lines = "#3 help\nsecond";
        require(write(writer, lines.data(), lines.size()) == static_cast<ssize_t>(lines.size()), "two lines written");
        static_cast<void>(close(writer));
        std::string line{};
        require((*door).read_command(line, 5000) == DoorWait::line && line == "#3 help", "the first line is read");
        require((*door).read_command(line, 50) == DoorWait::timed_out, "half a line waits, the writer's close no end");
        (*door).reply(door_reply_frame("hi\n.dot", "3", false));
        const int reader = open((folder + "/out").c_str(), O_RDONLY);
        require(reader >= 0, "a reader opens `out`");
        std::string got{};
        char buffer[256] = {};
        for (;;) {
            const ssize_t count = read(reader, buffer, sizeof(buffer));
            if (count <= 0) break;
            got.append(buffer, static_cast<std::size_t>(count));
        }
        static_cast<void>(close(reader));
        require(got == "hi\n..dot\n. #3 ok\n", "the reply reaches the reader, then its end: " + got);
        (*door).write_info("name: test\n");
        std::ifstream info(folder + "/info");
        std::string first{};
        std::string second{};
        std::getline(info, first);
        std::getline(info, second);
        require(first == "id: " + (*door).id() && second == "pid: " + std::to_string(getpid()),
                "info leads with the ID and the pid");
        (*door).interrupt();
        require((*door).read_command(line, 5000) == DoorWait::interrupted, "interrupt ends a wait");
    }
    struct stat gone {};
    require(lstat(folder.c_str(), &gone) != 0, "the folder goes with the door");
    remove_door_folder("../etc", 1);
}
#endif

int main() {
    opening_lines();
    console_commands();
    error_lines();
    info_entries();
    peer_rule();
    same_user_connection();
    ended_entries_are_removed();
    timed_accept_ends();
    interrupt_ends_accept();
    stalled_connection_is_aborted();
    door_lines();
#if !defined(_WIN32)
    door_round_trip();
#endif
    std::cout << "automation attach tests passed\n";
    return 0;
}
