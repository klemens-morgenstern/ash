#ifndef ASH_READER_HPP
#define ASH_READER_HPP

#include <array>
#include <string_view>
#include <ash/config.hpp>
#include <ash/tokenizer.hpp>

namespace ash
{

template<typename Executor = net::any_io_executor>
using basic_chunk_reader = net::experimental::coro<std::string_view, void, Executor>;
template<typename Executor = net::any_io_executor>
using basic_chunk_writer = net::experimental::coro<std::size_t(std::string_view), void, Executor>;

using chunk_reader = basic_chunk_reader<>;
using chunk_writer = basic_chunk_writer<>;

template<typename StreamType>
auto stream_reader(StreamType stream) -> basic_chunk_reader<typename std::decay_t<StreamType>::executor_type>
{
    while (stream.is_open())
    {
        std::array<char, 4096> buf;
        auto read = co_await stream.async_read_some(net::buffer(buf), net::experimental::use_coro);
        if (read == 0u)
            continue;

        co_yield std::string_view{buf.data(), read};
    }
}

template<typename StreamType>
auto stream_writer(StreamType stream, std::string_view msg = "") -> basic_chunk_writer<typename std::decay_t<StreamType>::executor_type>
{
    while (stream.is_open())
        msg = co_yield co_await async_write(stream, net::buffer(msg), net::experimental::use_coro);
}


struct reader_mode
{
    struct tokenize_t {};
    struct raw_line_t {};
    struct multiline_with_terminator_t {std::string_view terminator;};
    struct multiline_with_predicate_t {std::function<bool(std::string_view)> predicate;};

    std::variant<tokenize_t, raw_line_t, multiline_with_predicate_t, multiline_with_terminator_t> state;

    reader_mode(tokenize_t val = {}) : state(std::move(val)) {}
    reader_mode(raw_line_t val) : state(std::move(val)) {}
    reader_mode(multiline_with_terminator_t val) : state(std::move(val)) {}
    reader_mode(multiline_with_predicate_t val) : state(std::move(val)) {}

    bool tokenize() {return holds_alternative<tokenize_t>(state);}
    bool raw_line() {return holds_alternative<raw_line_t>(state);}
    auto multiline_with_terminator() {return get_if<multiline_with_terminator_t>(&state);}
    auto multiline_with_predicate()  {return get_if<multiline_with_predicate_t>(&state);}
};


template<typename Executor = net::any_io_executor>
using basic_token_reader = net::experimental::coro<tokenized_view(reader_mode), void, Executor>;

using token_reader = basic_token_reader<>;

template<typename Executor = net::any_io_executor>
basic_token_reader<Executor> read(basic_chunk_reader<Executor> reader, reader_mode mode = {})
{
    std::string buffer;
    while (auto msg_ = co_await reader)
    {
        auto msg = *msg_;
        if (!buffer.empty())
        {
            buffer.insert(buffer.end(), msg.begin(), msg.end());
            msg = buffer;
        }

        if (mode.tokenize())
        {
            auto [d, rest_line] = pick_line(msg);
            if (d.empty())
            {
                buffer.assign(rest_line.begin(), rest_line.end());
                continue;
            }
            auto [res, rest] = tokenize(d);
            buffer.assign(rest.begin(), rest.end());
            buffer += rest_line;
            mode = co_yield res;
        }
        else if (mode.raw_line())
        {
            if (auto pos = msg.find('\n'); pos != std::string_view::npos)
            {
                mode = co_yield {.raw_input = msg.substr(0, pos)};
                buffer.assign(msg.substr(pos));
            }
            else
                buffer.insert(buffer.end(), msg.begin(), msg.end());
        }
        else if (auto mlp = mode.multiline_with_predicate(); mlp != nullptr)
        {
            bool found = false;
            for (auto pos = msg.find('\n'); pos != std::string_view::npos; pos = msg.find('\n', pos + 1))
            {
                auto candidate = msg.substr(0, pos);
                if (mlp->predicate(candidate))
                {
                    mode = co_yield {.raw_input = candidate};
                    buffer.assign(msg.substr(pos + 1));
                    found = true;
                    break;
                }
            }
            if (!found)
                buffer.assign(msg.begin(), msg.end());
        }
        else if (auto mlt = mode.multiline_with_terminator(); mlt != nullptr)
        {
            bool found = false;
            for (auto pos = msg.find('\n'); pos != std::string_view::npos; pos = msg.find('\n', pos + 1))
            {
                auto candidate = msg.substr(0, pos);
                if (candidate.ends_with(mlt->terminator))
                {
                    mode = co_yield {.raw_input = candidate};
                    buffer.assign(msg.substr(pos + 1));
                    found = true;
                    break;
                }
            }
            if (!found)
                buffer.assign(msg.begin(), msg.end());
        }
    }
}



}

#endif //ASH_READER_HPP
