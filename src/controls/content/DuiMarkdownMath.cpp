/**
 * 文件名：DuiMarkdownMath.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：轻量 LaTeX 公式布局（Markdown $ / $$ 常用子集）。
 */
#include "DuiMarkdownAst.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

namespace ysDui::controls::content::detail {
namespace {

struct Atom final {
    enum class Kind { Text, Space, Frac, Binom, Sqrt, Script, Overline } kind{Kind::Text};
    std::string text;
    std::unique_ptr<Atom> left;  // frac/binom num / sqrt|overline body / script base
    std::unique_ptr<Atom> right; // frac/binom den / script script
    bool super{};                // script: true=^ false=_
    bool roman{};                // \text/\mathrm：直立
    bool bold{};                 // \mathbf
    int spaceEmTenths{-1};       // Space：以 0.1em 为单位；-1 表示默认空格
    std::vector<std::unique_ptr<Atom>> children; // row
};

[[nodiscard]] std::string MapCommand(std::string_view name)
{
    static const std::unordered_map<std::string, std::string> kMap{
        {"alpha", "α"},   {"beta", "β"},   {"gamma", "γ"}, {"delta", "δ"},
        {"epsilon", "ε"}, {"varepsilon", "ε"}, {"zeta", "ζ"}, {"eta", "η"},
        {"theta", "θ"},   {"vartheta", "ϑ"}, {"iota", "ι"}, {"kappa", "κ"},
        {"lambda", "λ"},  {"mu", "μ"},     {"nu", "ν"},    {"xi", "ξ"},
        {"pi", "π"},      {"rho", "ρ"},    {"sigma", "σ"}, {"tau", "τ"},
        {"upsilon", "υ"}, {"phi", "φ"},    {"varphi", "ϕ"}, {"chi", "χ"},
        {"psi", "ψ"},     {"omega", "ω"},
        {"Alpha", "Α"},   {"Beta", "Β"},   {"Gamma", "Γ"}, {"Delta", "Δ"},
        {"Theta", "Θ"},   {"Lambda", "Λ"}, {"Xi", "Ξ"},    {"Pi", "Π"},
        {"Sigma", "Σ"},   {"Phi", "Φ"},    {"Psi", "Ψ"},   {"Omega", "Ω"},
        {"times", "×"},   {"cdot", "·"},   {"pm", "±"},    {"mp", "∓"},
        {"div", "÷"},     {"ast", "∗"},
        {"leq", "≤"},     {"geq", "≥"},    {"neq", "≠"},   {"approx", "≈"},
        {"equiv", "≡"},   {"sim", "∼"},    {"propto", "∝"},
        {"infty", "∞"},   {"partial", "∂"}, {"nabla", "∇"},
        {"sum", "∑"},     {"prod", "∏"},   {"int", "∫"},   {"oint", "∮"},
        {"sqrt", "√"},
        {"forall", "∀"},  {"exists", "∃"}, {"neg", "¬"},   {"land", "∧"},
        {"lor", "∨"},     {"in", "∈"},     {"notin", "∉"}, {"subset", "⊂"},
        {"subseteq", "⊆"}, {"supset", "⊃"}, {"cup", "∪"},  {"cap", "∩"},
        {"emptyset", "∅"}, {"angle", "∠"}, {"perp", "⊥"}, {"parallel", "∥"},
        {"log", "log"},   {"ln", "ln"},    {"sin", "sin"}, {"cos", "cos"},
        {"tan", "tan"},   {"cot", "cot"},  {"sec", "sec"}, {"csc", "csc"},
        {"lim", "lim"},   {"max", "max"},  {"min", "min"},
        {"to", "→"},      {"rightarrow", "→"}, {"leftarrow", "←"},
        {"Rightarrow", "⇒"}, {"Leftarrow", "⇐"}, {"leftrightarrow", "↔"},
        {"uparrow", "↑"}, {"downarrow", "↓"},
        {"ldots", "…"},   {"cdots", "⋯"},  {"vdots", "⋮"}, {"ddots", "⋱"},
        {"hbar", "ℏ"},    {"ell", "ℓ"},    {"Re", "ℜ"},    {"Im", "ℑ"},
    };
    const auto it = kMap.find(std::string(name));
    return it != kMap.end() ? it->second : std::string(name);
}

[[nodiscard]] char32_t SuperChar(char ch)
{
    switch (ch)
    {
    case '0': return U'⁰';
    case '1': return U'¹';
    case '2': return U'²';
    case '3': return U'³';
    case '4': return U'⁴';
    case '5': return U'⁵';
    case '6': return U'⁶';
    case '7': return U'⁷';
    case '8': return U'⁸';
    case '9': return U'⁹';
    case '+': return U'⁺';
    case '-': return U'⁻';
    case 'n': return U'ⁿ';
    case 'i': return U'ⁱ';
    default: return 0;
    }
}

[[nodiscard]] char32_t SubChar(char ch)
{
    switch (ch)
    {
    case '0': return U'₀';
    case '1': return U'₁';
    case '2': return U'₂';
    case '3': return U'₃';
    case '4': return U'₄';
    case '5': return U'₅';
    case '6': return U'₆';
    case '7': return U'₇';
    case '8': return U'₈';
    case '9': return U'₉';
    case '+': return U'₊';
    case '-': return U'₋';
    case 'n': return U'ₙ';
    case 'i': return U'ᵢ';
    default: return 0;
    }
}

[[nodiscard]] std::string Utf8FromCodepoint(char32_t cp)
{
    std::string out;
    if (cp < 0x80)
        out.push_back(static_cast<char>(cp));
    else if (cp < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

[[nodiscard]] bool TryUnicodeScript(std::string_view text, bool super, std::string& out)
{
    out.clear();
    for (const char ch : text)
    {
        const char32_t mapped = super ? SuperChar(ch) : SubChar(ch);
        if (mapped == 0)
            return false;
        out += Utf8FromCodepoint(mapped);
    }
    return !out.empty();
}

void SkipWs(std::string_view& s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0)
        s.remove_prefix(1);
}

[[nodiscard]] std::string ParseGroup(std::string_view& s)
{
    SkipWs(s);
    if (s.empty())
        return {};
    if (s.front() == '{')
    {
        s.remove_prefix(1);
        std::string body;
        int depth = 1;
        while (!s.empty() && depth > 0)
        {
            if (s.front() == '{')
                ++depth;
            else if (s.front() == '}')
                --depth;
            if (depth == 0)
            {
                s.remove_prefix(1);
                break;
            }
            body.push_back(s.front());
            s.remove_prefix(1);
        }
        return body;
    }
    // 单 token
    if (s.front() == '\\')
    {
        s.remove_prefix(1);
        std::string name;
        while (!s.empty() && std::isalpha(static_cast<unsigned char>(s.front())) != 0)
        {
            name.push_back(s.front());
            s.remove_prefix(1);
        }
        return "\\" + name;
    }
    std::string one(1, s.front());
    s.remove_prefix(1);
    return one;
}

std::unique_ptr<Atom> ParseExpr(std::string_view& s);

std::unique_ptr<Atom> ParsePrimaryFixed(std::string_view& s)
{
    SkipWs(s);
    if (s.empty())
        return nullptr;
    if (s.front() == '\\')
    {
        s.remove_prefix(1);
        std::string name;
        while (!s.empty() && std::isalpha(static_cast<unsigned char>(s.front())) != 0)
        {
            name.push_back(s.front());
            s.remove_prefix(1);
        }
        if (name == "frac" || name == "binom")
        {
            auto atom = std::make_unique<Atom>();
            atom->kind = name == "binom" ? Atom::Kind::Binom : Atom::Kind::Frac;
            const std::string num = ParseGroup(s);
            const std::string den = ParseGroup(s);
            std::string_view nv = num;
            std::string_view dv = den;
            atom->left = ParseExpr(nv);
            atom->right = ParseExpr(dv);
            return atom;
        }
        if (name == "sqrt")
        {
            auto atom = std::make_unique<Atom>();
            atom->kind = Atom::Kind::Sqrt;
            const std::string body = ParseGroup(s);
            std::string_view bv = body;
            atom->left = ParseExpr(bv);
            return atom;
        }
        if (name == "overline")
        {
            auto atom = std::make_unique<Atom>();
            atom->kind = Atom::Kind::Overline;
            const std::string body = ParseGroup(s);
            std::string_view bv = body;
            atom->left = ParseExpr(bv);
            return atom;
        }
        if (name == "text" || name == "mathrm" || name == "operatorname" || name == "mathbf")
        {
            auto atom = std::make_unique<Atom>();
            atom->kind = Atom::Kind::Text;
            atom->roman = true;
            atom->bold = name == "mathbf";
            atom->text = ParseGroup(s);
            return atom;
        }
        if (name == "quad" || name == "qquad")
        {
            auto atom = std::make_unique<Atom>();
            atom->kind = Atom::Kind::Space;
            atom->spaceEmTenths = name == "qquad" ? 20 : 10;
            return atom;
        }
        if (name.empty() && !s.empty())
        {
            // \, \; \! 等间距
            const char mark = s.front();
            if (mark == ',' || mark == ';' || mark == '!')
            {
                s.remove_prefix(1);
                auto atom = std::make_unique<Atom>();
                atom->kind = Atom::Kind::Space;
                atom->spaceEmTenths = mark == ';' ? 5 : (mark == '!' ? 0 : 3);
                return atom;
            }
        }
        if (name == "left" || name == "right")
        {
            SkipWs(s);
            if (!s.empty())
            {
                auto atom = std::make_unique<Atom>();
                atom->kind = Atom::Kind::Text;
                atom->roman = true;
                if (s.front() == '\\')
                {
                    // \left\{ \right\}
                    s.remove_prefix(1);
                    if (!s.empty() && (s.front() == '{' || s.front() == '}'))
                    {
                        atom->text.assign(1, s.front());
                        s.remove_prefix(1);
                        return atom;
                    }
                    // 回退：把反斜杠后的命令当文本
                    std::string cmd;
                    while (!s.empty() && std::isalpha(static_cast<unsigned char>(s.front())) != 0)
                    {
                        cmd.push_back(s.front());
                        s.remove_prefix(1);
                    }
                    atom->text = MapCommand(cmd);
                    return atom;
                }
                atom->text.assign(1, s.front());
                s.remove_prefix(1);
                return atom;
            }
        }
        auto atom = std::make_unique<Atom>();
        atom->kind = Atom::Kind::Text;
        // 函数名直立显示
        if (name == "sin" || name == "cos" || name == "tan" || name == "cot" || name == "sec"
            || name == "csc" || name == "log" || name == "ln" || name == "lim" || name == "max"
            || name == "min" || name == "det" || name == "exp")
            atom->roman = true;
        atom->text = MapCommand(name);
        return atom;
    }
    if (s.front() == '{')
    {
        const std::string body = ParseGroup(s);
        std::string_view bv = body;
        return ParseExpr(bv);
    }
    auto atom = std::make_unique<Atom>();
    atom->kind = Atom::Kind::Text;
    if (std::isalnum(static_cast<unsigned char>(s.front())) != 0 || s.front() == '.')
    {
        while (!s.empty()
               && (std::isalnum(static_cast<unsigned char>(s.front())) != 0 || s.front() == '.'))
        {
            atom->text.push_back(s.front());
            s.remove_prefix(1);
        }
    }
    else
    {
        atom->text.assign(1, s.front());
        s.remove_prefix(1);
    }
    return atom;
}

std::unique_ptr<Atom> ParseExpr(std::string_view& s)
{
    auto row = std::make_unique<Atom>();
    row->kind = Atom::Kind::Text; // reuse children as row
    while (true)
    {
        SkipWs(s);
        if (s.empty() || s.front() == '}')
            break;
        auto primary = ParsePrimaryFixed(s);
        if (!primary)
            break;
        SkipWs(s);
        while (!s.empty() && (s.front() == '^' || s.front() == '_'))
        {
            const bool super = s.front() == '^';
            s.remove_prefix(1);
            auto script = std::make_unique<Atom>();
            script->kind = Atom::Kind::Script;
            script->super = super;
            script->left = std::move(primary);
            const std::string body = ParseGroup(s);
            std::string_view bv = body;
            script->right = ParseExpr(bv);
            primary = std::move(script);
            SkipWs(s);
        }
        row->children.push_back(std::move(primary));
    }
    if (row->children.size() == 1)
        return std::move(row->children.front());
    return row;
}

struct Box final {
    int width{};
    int height{};
    int baseline{};
    std::vector<LaidDecor> decors;
    std::vector<LaidRun> runs;
};

Box MeasureAtom(const Atom* atom, render::DuiTextMeasurer& measurer, render::DuiTextStyle style,
                float scale);

Box MeasureRow(const std::vector<std::unique_ptr<Atom>>& children, render::DuiTextMeasurer& measurer,
               const render::DuiTextStyle& style, float scale)
{
    Box box;
    int x = 0;
    int maxAbove = 0;
    int maxBelow = 0;
    std::vector<Box> parts;
    parts.reserve(children.size());
    for (const auto& child : children)
    {
        Box part = MeasureAtom(child.get(), measurer, style, scale);
        maxAbove = (std::max)(maxAbove, part.baseline);
        maxBelow = (std::max)(maxBelow, part.height - part.baseline);
        parts.push_back(std::move(part));
    }
    box.baseline = maxAbove;
    box.height = maxAbove + maxBelow;
    for (Box& part : parts)
    {
        const int dy = box.baseline - part.baseline;
        for (LaidDecor& decor : part.decors)
        {
            decor.bounds.left += x;
            decor.bounds.right += x;
            decor.bounds.top += dy;
            decor.bounds.bottom += dy;
            box.decors.push_back(std::move(decor));
        }
        for (LaidRun& run : part.runs)
        {
            run.bounds.left += x;
            run.bounds.right += x;
            run.bounds.top += dy;
            run.bounds.bottom += dy;
            box.runs.push_back(std::move(run));
        }
        x += part.width;
    }
    box.width = x;
    return box;
}

Box MeasureAtom(const Atom* atom, render::DuiTextMeasurer& measurer, render::DuiTextStyle style,
                float scale)
{
    Box box;
    if (atom == nullptr)
        return box;
    style.pointSize = (std::max)(8, static_cast<int>(std::lround(style.pointSize * scale)));
    style.italic = !atom->roman;
    if (atom->bold)
        style.bold = true;

    if (!atom->children.empty() && atom->kind == Atom::Kind::Text)
        return MeasureRow(atom->children, measurer, style, scale);

    switch (atom->kind)
    {
    case Atom::Kind::Space:
    {
        const int em = (std::max)(8, style.pointSize);
        const int tenths = atom->spaceEmTenths < 0 ? 4 : atom->spaceEmTenths;
        box.width = (std::max)(0, em * tenths / 10);
        box.height = (std::max)(style.pointSize + 4, em);
        box.baseline = box.height * 3 / 4;
        return box;
    }
    case Atom::Kind::Text:
    {
        const auto metrics = measurer.MeasureText(atom->text, style, {});
        box.width = metrics.size.width;
        box.height = (std::max)(style.pointSize + 4, metrics.size.height);
        box.baseline = box.height * 3 / 4;
        LaidRun run;
        run.bounds = {0, 0, box.width, box.height};
        run.text = atom->text;
        run.style = style;
        run.selectable = true;
        box.runs.push_back(std::move(run));
        return box;
    }
    case Atom::Kind::Overline:
    {
        Box body = MeasureAtom(atom->left.get(), measurer, style, scale);
        box.width = body.width + 2;
        box.height = body.height + 3;
        box.baseline = body.baseline + 3;
        LaidDecor bar;
        bar.bounds = {0, 0, box.width, 1};
        bar.fill = style.color;
        bar.fillOnly = true;
        box.decors.push_back(bar);
        for (LaidDecor& decor : body.decors)
        {
            decor.bounds.left += 1;
            decor.bounds.right += 1;
            decor.bounds.top += 3;
            decor.bounds.bottom += 3;
            box.decors.push_back(std::move(decor));
        }
        for (LaidRun& run : body.runs)
        {
            run.bounds.left += 1;
            run.bounds.right += 1;
            run.bounds.top += 3;
            run.bounds.bottom += 3;
            box.runs.push_back(std::move(run));
        }
        return box;
    }
    case Atom::Kind::Frac:
    case Atom::Kind::Binom:
    {
        const bool binom = atom->kind == Atom::Kind::Binom;
        Box num = MeasureAtom(atom->left.get(), measurer, style, scale * 0.85f);
        Box den = MeasureAtom(atom->right.get(), measurer, style, scale * 0.85f);
        const int gap = 3;
        const int rule = binom ? 0 : 1;
        const int innerW = (std::max)(num.width, den.width) + 8;
        box.height = num.height + den.height + gap * 2 + rule;
        box.baseline = num.height + gap + rule / 2;
        int contentX = 0;
        if (binom)
        {
            render::DuiTextStyle parenStyle = style;
            parenStyle.italic = false;
            const auto lp = measurer.MeasureText("(", parenStyle, {});
            const auto rp = measurer.MeasureText(")", parenStyle, {});
            contentX = lp.size.width;
            box.width = lp.size.width + innerW + rp.size.width;
            LaidRun leftParen;
            leftParen.bounds = {0, 0, lp.size.width, box.height};
            leftParen.text = "(";
            leftParen.style = parenStyle;
            leftParen.selectable = false;
            box.runs.push_back(std::move(leftParen));
            LaidRun rightParen;
            rightParen.bounds = {contentX + innerW, 0, box.width, box.height};
            rightParen.text = ")";
            rightParen.style = parenStyle;
            rightParen.selectable = false;
            box.runs.push_back(std::move(rightParen));
        }
        else
        {
            box.width = innerW;
        }
        const int numX = contentX + (innerW - num.width) / 2;
        const int denX = contentX + (innerW - den.width) / 2;
        for (LaidDecor& decor : num.decors)
        {
            decor.bounds.left += numX;
            decor.bounds.right += numX;
            box.decors.push_back(std::move(decor));
        }
        for (LaidRun& run : num.runs)
        {
            run.bounds.left += numX;
            run.bounds.right += numX;
            box.runs.push_back(std::move(run));
        }
        if (!binom)
        {
            LaidDecor line;
            line.bounds = {contentX + 2, num.height + gap, contentX + innerW - 2,
                           num.height + gap + rule};
            line.fill = style.color;
            line.fillOnly = true;
            box.decors.push_back(line);
        }
        const int denY = num.height + gap * 2 + rule;
        for (LaidDecor& decor : den.decors)
        {
            decor.bounds.left += denX;
            decor.bounds.right += denX;
            decor.bounds.top += denY;
            decor.bounds.bottom += denY;
            box.decors.push_back(std::move(decor));
        }
        for (LaidRun& run : den.runs)
        {
            run.bounds.left += denX;
            run.bounds.right += denX;
            run.bounds.top += denY;
            run.bounds.bottom += denY;
            box.runs.push_back(std::move(run));
        }
        return box;
    }
    case Atom::Kind::Sqrt:
    {
        Box body = MeasureAtom(atom->left.get(), measurer, style, scale);
        const int pad = 4;
        const int radicalW = (std::max)(10, style.pointSize);
        box.width = radicalW + body.width + pad;
        box.height = body.height + 4;
        box.baseline = body.baseline + 2;
        LaidDecor bar;
        bar.bounds = {radicalW, 1, box.width - 1, 2};
        bar.fill = style.color;
        bar.fillOnly = true;
        box.decors.push_back(bar);
        LaidDecor leg;
        leg.bounds = {radicalW - 2, 1, radicalW - 1, box.height - 1};
        leg.fill = style.color;
        leg.fillOnly = true;
        box.decors.push_back(leg);
        LaidRun tick;
        tick.bounds = {0, box.height / 3, radicalW - 2, box.height};
        tick.text = "√";
        tick.style = style;
        tick.style.italic = false;
        tick.selectable = false;
        box.runs.push_back(std::move(tick));
        for (LaidDecor& decor : body.decors)
        {
            decor.bounds.left += radicalW;
            decor.bounds.right += radicalW;
            decor.bounds.top += 3;
            decor.bounds.bottom += 3;
            box.decors.push_back(std::move(decor));
        }
        for (LaidRun& run : body.runs)
        {
            run.bounds.left += radicalW;
            run.bounds.right += radicalW;
            run.bounds.top += 3;
            run.bounds.bottom += 3;
            box.runs.push_back(std::move(run));
        }
        return box;
    }
    case Atom::Kind::Script:
    {
        Box base = MeasureAtom(atom->left.get(), measurer, style, scale);
        // 尝试 unicode 上下标
        if (atom->right && atom->right->kind == Atom::Kind::Text && atom->right->children.empty())
        {
            std::string mapped;
            if (TryUnicodeScript(atom->right->text, atom->super, mapped))
            {
                auto flat = std::make_unique<Atom>();
                flat->kind = Atom::Kind::Text;
                flat->text = base.runs.empty() ? std::string{} : base.runs.front().text;
                // rebuild as text concat
                Box scriptBox;
                render::DuiTextStyle st = style;
                const std::string combined =
                    (base.runs.empty() ? std::string{} : base.runs.front().text) + mapped;
                // If base had multiple runs, fall through
                if (base.runs.size() == 1 && base.decors.empty())
                {
                    const auto metrics = measurer.MeasureText(combined, st, {});
                    scriptBox.width = metrics.size.width;
                    scriptBox.height = (std::max)(st.pointSize + 4, metrics.size.height);
                    scriptBox.baseline = scriptBox.height * 3 / 4;
                    LaidRun run;
                    run.bounds = {0, 0, scriptBox.width, scriptBox.height};
                    run.text = combined;
                    run.style = st;
                    run.selectable = true;
                    scriptBox.runs.push_back(std::move(run));
                    return scriptBox;
                }
            }
        }
        Box script = MeasureAtom(atom->right.get(), measurer, style, scale * 0.72f);
        box.width = base.width + script.width;
        if (atom->super)
        {
            box.height = (std::max)(base.height, script.height + base.height / 3);
            box.baseline = box.height - (base.height - base.baseline);
            for (auto& decor : base.decors)
            {
                decor.bounds.top += box.height - base.height;
                decor.bounds.bottom += box.height - base.height;
                box.decors.push_back(std::move(decor));
            }
            for (auto& run : base.runs)
            {
                run.bounds.top += box.height - base.height;
                run.bounds.bottom += box.height - base.height;
                box.runs.push_back(std::move(run));
            }
            for (auto& decor : script.decors)
            {
                decor.bounds.left += base.width;
                decor.bounds.right += base.width;
                box.decors.push_back(std::move(decor));
            }
            for (auto& run : script.runs)
            {
                run.bounds.left += base.width;
                run.bounds.right += base.width;
                box.runs.push_back(std::move(run));
            }
        }
        else
        {
            box.height = (std::max)(base.height, script.height + base.height / 4);
            box.baseline = base.baseline;
            box.decors.insert(box.decors.end(), base.decors.begin(), base.decors.end());
            box.runs.insert(box.runs.end(), base.runs.begin(), base.runs.end());
            const int sy = box.height - script.height;
            for (auto& decor : script.decors)
            {
                decor.bounds.left += base.width;
                decor.bounds.right += base.width;
                decor.bounds.top += sy;
                decor.bounds.bottom += sy;
                box.decors.push_back(std::move(decor));
            }
            for (auto& run : script.runs)
            {
                run.bounds.left += base.width;
                run.bounds.right += base.width;
                run.bounds.top += sy;
                run.bounds.bottom += sy;
                box.runs.push_back(std::move(run));
            }
        }
        return box;
    }
    }
    return box;
}

} // namespace

MathFragment LayoutMathFragment(std::string_view latex, render::DuiTextMeasurer& measurer,
                                const render::DuiTextStyle& baseStyle, bool display)
{
    std::string_view cursor = latex;
    auto root = ParseExpr(cursor);
    Box box = MeasureAtom(root.get(), measurer, baseStyle, 1.0f);
    if (box.width == 0 && box.height == 0)
    {
        // 回退：原样斜体
        render::DuiTextStyle style = baseStyle;
        style.italic = true;
        const auto metrics = measurer.MeasureText(latex, style, {});
        box.width = metrics.size.width;
        box.height = (std::max)(style.pointSize + 6, metrics.size.height);
        box.baseline = box.height * 3 / 4;
        LaidRun run;
        run.bounds = {0, 0, box.width, box.height};
        run.text = std::string(latex);
        run.style = style;
        run.selectable = true;
        box.runs.push_back(std::move(run));
    }

    MathFragment fragment;
    fragment.size = {box.width + (display ? 0 : 2), box.height + (display ? 8 : 2)};
    fragment.baseline = box.baseline + (display ? 4 : 1);
    fragment.decors = std::move(box.decors);
    fragment.runs = std::move(box.runs);
    const int ox = display ? 0 : 1;
    const int oy = display ? 4 : 1;
    for (LaidDecor& decor : fragment.decors)
    {
        decor.bounds.left += ox;
        decor.bounds.right += ox;
        decor.bounds.top += oy;
        decor.bounds.bottom += oy;
    }
    for (LaidRun& run : fragment.runs)
    {
        run.bounds.left += ox;
        run.bounds.right += ox;
        run.bounds.top += oy;
        run.bounds.bottom += oy;
        run.plainBegin = run.plainEnd = 0;
    }
    (void)display;
    return fragment;
}

} // namespace ysDui::controls::content::detail
