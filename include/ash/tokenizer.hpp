/*
Copyright ©2021 Specta Omnis LLC
Copyright ©2021 Richard Hodges (richard@power.trade)

All Rights Reserved.

IN NO EVENT SHALL SPECTRA OMNIS LLC or RICHARD HODGES BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
EVEN IF SPECTRA OMNIS LLC OR RICHARD HODGES HAS BEEN ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.

SPECTRA OMNIS LLC AND RICHARD HODGES SPECIFICALLY DISCLAIMS ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO
PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
#ifndef ASH_TOKENIZER_HPP
#define ASH_TOKENIZER_HPP

#include <ctre.hpp>
#include <vector>
#include <vector>
namespace ash
{

constexpr auto line_matcher = ctre::starts_with<R"rx(\s*((?:"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|#[^\n]*\n|\\[^\n]*\n|[^;\n])*)(?:;|\n))rx">;

constexpr std::pair<std::string_view, std::string_view> pick_line(std::string_view raw)
{
    auto ln = line_matcher(raw);
    if (!ln)
        return {"", raw};
    else
        return {get<1>(ln).view(), raw.substr(ln.size())};
}

constexpr auto tokenizer = ctre::tokenize<R"rx("((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'|#([^\n]*)\n|(\\)|(;)|(\n)|([^\s"']+)|(\s+))rx">;

struct tokenized_view
{
    std::string_view raw_input;
    std::vector<std::string_view> tokens;
};

inline std::pair<tokenized_view, std::string_view> tokenize(std::string_view raw, bool skip_ws = true)
{
    /*
     regexes:
        double quoted string: "((?:[^"\\]|\\.)*)"
        single quoted string: '(?:[^'\\]|\\.)*'
        comment: #([^\n])*
        line extension: (\\)
        semi-colon: ';'
        line-break: \n
        regular token: ([^\s"']+)
        whitespace: (\s+)
     */
    std::size_t offset{0u};
    tokenized_view res;
    for (auto tk : tokenizer(raw))
    {
        auto & [full, double_quoted, single_quoted, comment, line_extension, semi_colon, line_break, regular, whitespace] = tk;
        offset += full.size();

        if (skip_ws && (comment || line_extension || semi_colon || line_break || whitespace))
            continue;

        if(double_quoted) res.tokens.push_back(double_quoted.view());
        else if(single_quoted) res.tokens.push_back(single_quoted.view());
        else if(comment) res.tokens.push_back(comment.view());
        else if(line_extension) res.tokens.push_back(line_extension.view());
        else if(semi_colon) res.tokens.push_back(semi_colon.view());
        else if(line_break) res.tokens.push_back(line_break.view());
        else if(regular) res.tokens.push_back(regular.view());
        else if(whitespace) res.tokens.push_back(whitespace.view());
    }
    res.raw_input =  raw.substr(0, offset);
    return {res, raw.substr(offset)};
}

}

#endif //ASH_TOKENIZER_HPP
