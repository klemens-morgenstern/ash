#ifndef ASH_SHELL_HPP
#define ASH_SHELL_HPP

#include <ash/config.hpp>
#include <ash/reader.hpp>

#include <map>
#include <functional>

namespace ash
{

template<typename Executor = net::any_io_executor>
using basic_cmd_task = net::experimental::coro<void, void, Executor>;

template<typename Executor>
struct basic_context;

template<typename Executor>
struct basic_shell;

template<typename Executor = net::any_io_executor>
struct basic_cmd
{
    using executor_type = Executor;
    using context_type = basic_context<executor_type>;
    using cmd_task = basic_cmd_task<executor_type>;

    std::string name;
    std::vector<std::string> aliases;

    std::function<cmd_task(context_type)> run;
    std::string help;
    std::string description;

    std::vector<basic_cmd<executor_type>> children;
};

template<typename Executor = net::any_io_executor>
struct basic_shell
{
    using executor_type = Executor;
    using cmd_task = basic_cmd_task<executor_type>;
    using cmd = basic_cmd<executor_type>;
    using chunk_reader = basic_chunk_reader<executor_type>;
    using chunk_writer = basic_chunk_writer<executor_type>;

    using shell_task = net::experimental::coro<void, void, executor_type >;
    using token_reader = basic_token_reader<executor_type>;

    executor_type get_executor() const {return reader_.get_executor();}

    basic_shell(chunk_reader reader, chunk_writer writer, const std::string  & prompt = "ash")
            : reader_(read(std::move(reader))), writer_(std::move(writer)), prompt_(prompt) {}

    basic_shell(
            executor_type exec, const std::vector<cmd> & cmds,
            int fd_source = STDIN_FILENO, int fd_sink = STDOUT_FILENO, const std::string  & prompt = "ash") :
            cmds_(cmds),  prompt_(prompt + "> "),
            reader_(read(stream_reader<net::posix::basic_stream_descriptor<Executor>>({exec, fd_source}))),
            writer_(stream_writer<net::posix::basic_stream_descriptor<Executor>>({exec, fd_sink}, prompt_))
    {}

    basic_shell(
            net::ip::tcp::socket & sock, const std::vector<cmd> & cmds, const std::string  & prompt = "ash") :
            cmds_(cmds),
            prompt_(prompt + "> "),
            reader_(read(stream_reader<net::ip::tcp::socket &>(sock))),
            writer_(stream_writer<net::ip::tcp::socket &>(sock, prompt_))
    {}

    template<typename Handler>
    auto async_run(Handler && handler)
    {
        return task_.async_resume(std::forward<Handler>(handler));
    }

    auto clear_screen() {return writer_("\e[1;1H\e[2J");}
    auto write(std::string_view data) {return writer_(data);}

    auto read_line() -> net::experimental::coro<void, std::string_view, Executor>
    {
        auto v = co_await reader_(reader_mode{reader_mode::raw_line_t{}});
        if (!v)
            co_return "";
        else
            co_return v->raw_input;
    }

    auto read_tokenized()
    {
        return reader_(reader_mode{reader_mode::tokenize_t{}});
    }
    auto read_multiline(std::string_view eoi) -> net::experimental::coro<void, std::string_view, Executor>
    {
        auto v = co_await reader_(reader_mode::multiline_with_terminator_t{eoi});
        if (!v)
            co_return "";
        else
            co_return v->raw_input;
    }
    auto read_multiline(std::function<bool(std::string_view)> predicate) -> net::experimental::coro<void, std::string_view, Executor>
    {
        auto v = co_await reader_(reader_mode::multiline_with_predicate_t{predicate});
        if (!v)
            co_return "";
        else
            co_return v->raw_input;
    }

    void set_prompt(std::string_view sv)
    {
        prompt_.assign(sv.begin(), sv.end());
        prompt_ += "> ";
    }
    std::string_view get_prompt() const {return std::string_view(prompt_).substr(0, prompt_.size() - 2);}
  private:
    executor_type executor_;

    std::vector<cmd> cmds_;
    std::string prompt_;

    token_reader reader_;
    chunk_writer writer_;

    shell_task task_impl_();
    shell_task task_{task_impl_()};

    std::string build_help_(const std::vector<std::string_view> & tk);
};

template<typename Executor = net::any_io_executor>
struct basic_context
{
    using executor_type = Executor;

    executor_type get_executor() const {return shell.get_executor();}

    std::vector<std::string_view> &args;
    std::string_view &raw_line;
    std::vector<std::string_view> &full_args;

    basic_shell<Executor> &shell;

    auto clear_screen() {return shell.clear_screen(); }
    auto write(std::string_view data) {return shell.write(data);}
    auto read_line() {return shell.read_line(); }
    auto read_tokenized() {return shell.read_tokenized(); }
    auto read_multiline(std::string_view eoi) {return shell.read_multiline(eoi); }
    auto read_multiline(std::function<bool(std::string_view)> predicate) {return shell.read_multiline(predicate); }
};

template<typename Executor, typename Iterator>
inline std::pair<const basic_cmd<Executor>*, std::size_t>
    find_command(Iterator begin, Iterator end,
                 const std::vector<basic_cmd<Executor>> & cmds,
                 std::size_t depth = 0u)
{

    auto nx = *begin;
    auto cmd_itr = std::find_if(cmds.begin(), cmds.end(),
                                [&](auto & c)
                                {
                                    return c.name == nx
                                         || std::find(c.aliases.begin(), c.aliases.end(), nx) != c.aliases.end();
                                });

    if (cmd_itr != cmds.end())
    {
        //found a command, see if I can find a a nested one
        auto nested = find_command(std::next(begin), end, cmd_itr->children, depth + 1);
        if (nested.first)
            return nested;
        else
            return {&*cmd_itr, depth + 1};
    }
    else
        return {nullptr, depth};
}


template<typename Executor>
std::string basic_shell<Executor>::build_help_(const std::vector<std::string_view> & tk)
{
    if (tk.size() > 1)
    {
        auto [cmd, depth] = find_command(std::next(tk.begin()) , tk.end(), cmds_);
        if (!cmd)
            return "cannot find command help was requested for.\n";

        std::string res = cmd->help + "\n   " + cmd->description + "\n";
        if (!cmd->children.empty())
        {
            res +=  "  subcommands:";
            for (const auto & c : cmd->children)
            {
                res += "\n    - " + c.name + "\n        " + c.help;
                if (!c.aliases.empty())
                {
                    res += "\n      aliases:";
                    for (const auto &a : c.aliases)
                        res += "\n        - " + a;
                }
            }
        }
        return res + "\n";
    }
    else
    {
        std::string res = R"(
root commands:
    - help [cmds...]
        print this help.
    - exit
        end this program)";

        for (const auto & c : cmds_)
        {
            res += "\n    - " + c.name + "\n        " + c.help;
            if (!c.aliases.empty())
            {
                res += "\n      aliases:";
                for (const auto &a : c.aliases)
                    res += "\n        - " + a;
            }
        }

        return res + "\n";
    }
}


template<typename Executor>
auto basic_shell<Executor>::task_impl_() -> shell_task
{
    while (true)
    {
        co_await writer_(prompt_);
        auto cmd_ = co_await reader_(reader_mode());

        if (!cmd_)
            break;
        auto & cc = *cmd_;

        if (cc.tokens.empty())
            continue;

        auto nm = cc.tokens.front();
        if (nm == "exit")
            break;
        else if (nm == "help")
        {
            auto msg = build_help_(cc.tokens);
            co_await writer_(msg);
        }
        else if (auto [cd, depth] = find_command(cc.tokens.begin(), cc.tokens.end(), cmds_); cd != nullptr)
        {
            std::vector<std::string_view> args;
            args.resize(cc.tokens.size() - depth);
            std::copy(cc.tokens.begin() + depth, cc.tokens.end(), args.begin());

            co_await cd->run({ args, cc.raw_input, cc.tokens, *this});
        }
        else
            co_await writer_("command not found\n");
    }

    co_return;
}


using cmd_task = basic_cmd_task<>;
using shell    = basic_shell<>;
using cmd      = basic_cmd<>;
using context  = basic_context<>;

}

#endif //ASH_SHELL_HPP
