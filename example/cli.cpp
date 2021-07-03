#include <ash/config.hpp>


#if defined(BOOST_CAMPBELL)
#include <boost/asio/posix/stream_descriptor.hpp>
#else
#include <asio/posix/stream_descriptor.hpp>
#endif

#include <ash/shell.hpp>

auto run_demo(ash::context ctx) -> ash::cmd_task
{
    co_await ctx.write("Insert a line of raw text: ");
    auto line = co_await ctx.read_line();
    co_await ctx.write("You typed in '" + std::string(line) + "'\n");

    co_await ctx.write("Insert a line of text to be tokenized: ");
    auto tks = co_await ctx.read_tokenized();

    auto msg = "Your raw text '" + std::string(tks.value().raw_input) + "'\n"
               + " which gets tokenized to:\n";
    for (auto & tk : tks.value().tokens)
    msg += " - '" + std::string(tk) + "'\n";
    co_await ctx.write(msg);

    co_await ctx.write("Insert a multi-line text with terminator 'EOI':\n");
    auto ml = co_await ctx.read_multiline("EOI");
    msg = "You wrote '" + std::string(ml) + "'\n";
    co_await ctx.write(msg);

    co_await ctx.write("Insert a multi-line text with a predicate (first char == end char):\n");
    ml = co_await ctx.read_multiline(
            [](std::string_view sv)
            {
                return sv.front() == sv.back();
            });
    msg = "You wrote '" + std::string(ml) + "'\n";
    co_await ctx.write(msg);
}

int main(int argc, char * argv[])
{
    asio::io_context ctx;

    ash::shell sh{ctx.get_executor(),
                  {ash::cmd{
                    .name="demo",
                    .run=run_demo,
                    .help="this is a demo command",
                    .description="Some more description",
                    .children={{.name="clear",
                                .run = [](ash::context ctx) -> ash::cmd_task { co_await ctx.clear_screen(); },
                                .help="this is a nested demo command",
                                .description="clear the screen"}}
                  }}
    };

    sh.async_run(ash::net::detached);
    ctx.run();
    return 0;
}