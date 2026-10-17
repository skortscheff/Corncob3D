// asmdata - Extracts the initialized data segment (DGROUP) from the original MASM
// sources and emits it as C++.
//
// Usage: asmdata <srcdir> <out.cpp> <out.hpp> <module>[:DEF=val,...] ...
//
// Modules are processed in link order (3S.LNK); their .dataseg contents are
// concatenated (word aligned) into one image, the way the linker built DGROUP.
// Supported: include, IF/IFE/IFDEF/IFNDEF/IFIDNI/IFDIFI/ELSE/ELSEIF*/ENDIF,
// MACRO (with parameters) / REPT / ENDM, EQU and = (numeric, lazy) and EQU <text>
// aliases, DB/DW/DD with N DUP(...), strings and '?', LABEL/.label, EVEN, and
// expressions with + - * / MOD SHL SHR AND OR XOR NOT EQ NE LT LE GT GE OFFSET
// SEG LOW HIGH and $. Code segments are scanned only for EQU/IF/MACRO/INCLUDE.
// References to code labels inside data (jump tables, routine pointers) become
// "code refs" with value 0, to be bound to C++ functions by name.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\f\x1a");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n\f\x1a");
    return s.substr(a, b - a + 1);
}

std::string stripComment(const std::string& s) {
    char q = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (q) { if (c == q) q = 0; continue; }
        if (c == '\'' || c == '"') q = c;
        else if (c == ';') return s.substr(0, i);
    }
    return s;
}

bool isIdStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '@' || c == '$' || c == '?'; }
bool isIdChar(char c) { return isIdStart(c) || std::isdigit(static_cast<unsigned char>(c)); }

// Splits on commas not inside quotes, parentheses or <...>
std::vector<std::string> splitTop(const std::string& s, char sep = ',') {
    std::vector<std::string> out;
    int depth = 0;
    char q = 0;
    std::string cur;
    for (char c : s) {
        if (q) { cur += c; if (c == q) q = 0; continue; }
        if (c == '\'' || c == '"') { q = c; cur += c; continue; }
        if (c == '(' || c == '<') depth++;
        if (c == ')' || c == '>') depth--;
        if (c == sep && depth == 0) { out.push_back(trim(cur)); cur.clear(); continue; }
        cur += c;
    }
    if (!trim(cur).empty() || !out.empty()) out.push_back(trim(cur));
    return out;
}

struct Val {
    long long v = 0;
    bool known = true;      // false: depends on something unresolved this pass
    std::string codeRef;    // set when the value refers to a code label
};

struct Macro {
    std::vector<std::string> params;
    std::vector<std::string> body;
};

struct Module;

struct Global {
    std::map<std::string, long long> publicLabels; // lowercase name -> DGROUP offset
    std::map<std::string, std::string> labelModule;
    bool finalPass = false;
};

struct Warn {
    int count = 0;
    void operator()(const std::string& m) {
        if (count++ < 200) std::fprintf(stderr, "asmdata: %s\n", m.c_str());
    }
};
Warn warn;

struct CodeRef { long long offset; std::string name; int size; };

struct Module {
    std::string name;
    std::string srcdir;
    std::map<std::string, long long> defines;   // -D style
    std::map<std::string, std::string> equExpr; // numeric equ (lazy)
    std::map<std::string, std::string> textEqu; // EQU <text>
    std::map<std::string, long long> labels;    // data labels -> DGROUP offset
    std::set<std::string> codeLabels;           // procs / code labels seen
    std::map<std::string, Macro> macros;
    std::set<std::string> evaluating;
    Global* g = nullptr;
    long long base = 0;
    long long pc = 0;   // current DGROUP offset
    bool inData = false;
    std::vector<uint8_t>* image = nullptr;
    std::vector<CodeRef>* codeRefs = nullptr;
    bool emit = false;  // pass 2
    std::string file;
    int line = 0;

    std::string where() const { return name + ":" + file + ":" + std::to_string(line); }

    // ---------------- expressions ----------------
    struct Lexer {
        std::string s;
        size_t i = 0;
        std::string tok;
        enum Kind { END, NUM, ID, OP, STR } kind = END;
        long long num = 0;
        void next() {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
            if (i >= s.size()) { kind = END; tok.clear(); return; }
            char c = s[i];
            if (std::isdigit(static_cast<unsigned char>(c))) {
                size_t j = i;
                while (j < s.size() && std::isalnum(static_cast<unsigned char>(s[j]))) j++;
                std::string t = lower(s.substr(i, j - i));
                i = j;
                kind = NUM;
                tok = t;
                char suf = t.back();
                std::string body = t;
                int base = 10;
                if (suf == 'h') { base = 16; body.pop_back(); }
                else if ((suf == 'b' || suf == 'y') && t.find_first_not_of("01by") == std::string::npos) { base = 2; body.pop_back(); }
                else if (suf == 'o' || suf == 'q') { base = 8; body.pop_back(); }
                else if (suf == 'd' && t.find_first_not_of("0123456789d") == std::string::npos) { body.pop_back(); }
                num = std::strtoll(body.c_str(), nullptr, base);
                return;
            }
            if (isIdStart(c)) {
                size_t j = i;
                while (j < s.size() && isIdChar(s[j])) j++;
                tok = s.substr(i, j - i);
                i = j;
                kind = ID;
                return;
            }
            if (c == '\'' || c == '"') {
                size_t j = s.find(c, i + 1);
                if (j == std::string::npos) j = s.size();
                tok = s.substr(i + 1, j - i - 1);
                i = std::min(j + 1, s.size());
                kind = STR;
                return;
            }
            kind = OP;
            tok = std::string(1, c);
            i++;
        }
    };

    Val lookup(const std::string& rawName) {
        std::string n = lower(rawName);
        Val r;
        if (n == "$") { r.v = pc; return r; } // same frame as labels (DGROUP offsets)
        if (n == "@dataseg" || n == "@codeseg" || n == "@data" || n == "@code" || n == "dgroup") { r.v = 0; return r; }
        auto te = textEqu.find(n);
        if (te != textEqu.end()) return eval(te->second);
        auto d = defines.find(n);
        if (d != defines.end()) { r.v = d->second; return r; }
        auto e = equExpr.find(n);
        if (e != equExpr.end()) {
            if (evaluating.count(n)) throw std::runtime_error("recursive equ " + n);
            evaluating.insert(n);
            Val v = eval(e->second);
            evaluating.erase(n);
            return v;
        }
        auto l = labels.find(n);
        if (l != labels.end()) { r.v = l->second; return r; }
        auto p = g->publicLabels.find(n);
        if (p != g->publicLabels.end()) { r.v = p->second; return r; }
        // Unknown: a code label/proc, an extern, or a forward data label
        if (!g->finalPass) { r.known = false; return r; }
        r.codeRef = rawName;
        r.v = 0;
        return r;
    }

    Val eval(const std::string& expr) {
        Lexer lx;
        lx.s = expr;
        lx.next();
        Val v = parseOr(lx);
        if (lx.kind != Lexer::END) throw std::runtime_error("trailing '" + lx.tok + "' in expr: " + expr);
        return v;
    }

    static bool kw(const Lexer& lx, const char* k) { return lx.kind == Lexer::ID && lower(lx.tok) == k; }
    static Val combine(Val a, const Val& b, long long v) {
        a.v = v;
        a.known = a.known && b.known;
        if (a.codeRef.empty()) a.codeRef = b.codeRef;
        return a;
    }

    Val parseOr(Lexer& lx) {
        Val a = parseAnd(lx);
        while (kw(lx, "or") || kw(lx, "xor")) {
            bool isXor = kw(lx, "xor");
            lx.next();
            Val b = parseAnd(lx);
            a = combine(a, b, isXor ? (a.v ^ b.v) : (a.v | b.v));
        }
        return a;
    }
    Val parseAnd(Lexer& lx) {
        Val a = parseNot(lx);
        while (kw(lx, "and")) {
            lx.next();
            Val b = parseNot(lx);
            a = combine(a, b, a.v & b.v);
        }
        return a;
    }
    Val parseNot(Lexer& lx) {
        if (kw(lx, "not")) {
            lx.next();
            Val a = parseNot(lx);
            a.v = ~a.v;
            return a;
        }
        return parseRel(lx);
    }
    Val parseRel(Lexer& lx) {
        Val a = parseAdd(lx);
        for (;;) {
            std::string k = lx.kind == Lexer::ID ? lower(lx.tok) : "";
            if (k != "eq" && k != "ne" && k != "lt" && k != "le" && k != "gt" && k != "ge") return a;
            lx.next();
            Val b = parseAdd(lx);
            bool t = k == "eq" ? a.v == b.v : k == "ne" ? a.v != b.v : k == "lt" ? a.v < b.v
                   : k == "le" ? a.v <= b.v : k == "gt" ? a.v > b.v : a.v >= b.v;
            a = combine(a, b, t ? -1 : 0);
        }
    }
    Val parseAdd(Lexer& lx) {
        Val a = parseMul(lx);
        while (lx.kind == Lexer::OP && (lx.tok == "+" || lx.tok == "-")) {
            bool plus = lx.tok == "+";
            lx.next();
            Val b = parseMul(lx);
            a = combine(a, b, plus ? a.v + b.v : a.v - b.v);
        }
        return a;
    }
    Val parseMul(Lexer& lx) {
        Val a = parseUnary(lx);
        for (;;) {
            if (lx.kind == Lexer::OP && (lx.tok == "*" || lx.tok == "/")) {
                bool mul = lx.tok == "*";
                lx.next();
                Val b = parseUnary(lx);
                long long r = mul ? a.v * b.v : (b.v ? a.v / b.v : 0);
                a = combine(a, b, r);
            } else if (kw(lx, "mod") || kw(lx, "shl") || kw(lx, "shr")) {
                std::string k = lower(lx.tok);
                lx.next();
                Val b = parseUnary(lx);
                long long r = k == "mod" ? (b.v ? a.v % b.v : 0) : k == "shl" ? (a.v << b.v) : (a.v >> b.v);
                a = combine(a, b, r);
            } else {
                return a;
            }
        }
    }
    Val parseUnary(Lexer& lx) {
        if (lx.kind == Lexer::OP && lx.tok == "-") { lx.next(); Val a = parseUnary(lx); a.v = -a.v; return a; }
        if (lx.kind == Lexer::OP && lx.tok == "+") { lx.next(); return parseUnary(lx); }
        if (kw(lx, "offset")) { lx.next(); return parseUnary(lx); }
        if (kw(lx, "seg")) { lx.next(); Val a = parseUnary(lx); a.v = 0; a.codeRef.clear(); return a; }
        if (kw(lx, "low")) { lx.next(); Val a = parseUnary(lx); a.v &= 0xFF; return a; }
        if (kw(lx, "high")) { lx.next(); Val a = parseUnary(lx); a.v = (a.v >> 8) & 0xFF; return a; }
        if (kw(lx, "word") || kw(lx, "byte") || kw(lx, "dword") || kw(lx, "near") || kw(lx, "far")) {
            lx.next();
            if (kw(lx, "ptr")) lx.next();
            return parseUnary(lx);
        }
        return parsePrimary(lx);
    }
    Val parsePrimary(Lexer& lx) {
        Val r;
        if (lx.kind == Lexer::NUM) { r.v = lx.num; lx.next(); return r; }
        if (lx.kind == Lexer::STR) {
            for (char c : lx.tok) r.v = (r.v << 8) | static_cast<unsigned char>(c);
            lx.next();
            return r;
        }
        if (lx.kind == Lexer::OP && (lx.tok == "(" || lx.tok == "[")) {
            std::string close = lx.tok == "(" ? ")" : "]";
            lx.next();
            r = parseOr(lx);
            if (lx.tok != close) throw std::runtime_error("expected " + close);
            lx.next();
            return r;
        }
        if (lx.kind == Lexer::ID) {
            std::string name = lx.tok;
            lx.next();
            // seg:sym override - the segment part contributes nothing
            if (lx.kind == Lexer::OP && lx.tok == ":") {
                lx.next();
                return parseUnary(lx);
            }
            return lookup(name);
        }
        throw std::runtime_error("unexpected '" + lx.tok + "'");
    }

    long long evalNum(const std::string& e) {
        Val v = eval(e);
        return v.v;
    }

    // ---------------- data emission ----------------
    void putBytes(long long v, int size, const std::string& codeRef) {
        if (emit) {
            size_t need = static_cast<size_t>(pc + size);
            if (image->size() < need) image->resize(need, 0);
            for (int k = 0; k < size; k++) (*image)[pc + k] = static_cast<uint8_t>((v >> (8 * k)) & 0xFF);
            if (!codeRef.empty()) codeRefs->push_back({pc, codeRef, size});
        }
        pc += size;
    }

    void emitItems(const std::string& list, int size) {
        for (const std::string& itemRaw : splitTop(list)) {
            std::string item = trim(itemRaw);
            if (item.empty()) continue;
            std::string li = lower(item);
            // N dup (...)
            size_t dp = li.find(" dup");
            if (dp == std::string::npos) dp = li.find("dup");
            if (dp != std::string::npos) {
                size_t open = item.find('(', dp);
                size_t close = item.rfind(')');
                if (open != std::string::npos && close != std::string::npos && close > open &&
                    trim(li.substr(dp, open - dp)) == "dup") {
                    long long n = evalNum(item.substr(0, dp));
                    std::string inner = item.substr(open + 1, close - open - 1);
                    for (long long k = 0; k < n; k++) emitItems(inner, size);
                    continue;
                }
            }
            if (item == "?") { putBytes(0, size, ""); continue; }
            if ((item.front() == '\'' || item.front() == '"') && item.back() == item.front() && size == 1) {
                for (size_t k = 1; k + 1 < item.size(); k++) putBytes(static_cast<unsigned char>(item[k]), 1, "");
                continue;
            }
            Val v = eval(item);
            putBytes(v.v, size, v.codeRef);
        }
    }

    // ---------------- statements ----------------
    std::vector<int> ifStack; // 1 = active, 0 = inactive, 2 = already taken (inactive)
    bool active() const {
        for (int s : ifStack) if (s != 1) return false;
        return true;
    }

    bool condIdn(const std::string& rest, bool same) {
        auto parts = splitTop(rest);
        if (parts.size() < 2) return false;
        auto strip = [](std::string s) {
            s = trim(s);
            if (s.size() >= 2 && s.front() == '<' && s.back() == '>') s = s.substr(1, s.size() - 2);
            return lower(trim(s));
        };
        std::string a = strip(parts[0]), b = strip(parts[1]);
        // <_MODEL> style refers to the text equ's value
        auto te = textEqu.find(a);
        if (te != textEqu.end()) a = lower(te->second);
        bool eq = a == b;
        return same ? eq : !eq;
    }

    bool evalCond(const std::string& dir, const std::string& rest) {
        try {
            if (dir == "if") return evalNum(rest) != 0;
            if (dir == "ife") return evalNum(rest) == 0;
            if (dir == "ifdef") return lookupDefined(trim(rest));
            if (dir == "ifndef") return !lookupDefined(trim(rest));
            if (dir == "ifidni" || dir == "ifidn") return condIdn(rest, true);
            if (dir == "ifdifi" || dir == "ifdif") return condIdn(rest, false);
            if (dir == "ifb") return trim(rest) == "<>";
            if (dir == "ifnb") return trim(rest) != "<>";
        } catch (const std::exception& ex) {
            warn(where() + ": condition '" + rest + "': " + ex.what() + " (treated as false)");
        }
        return false;
    }

    bool lookupDefined(const std::string& n) {
        std::string l = lower(n);
        return defines.count(l) || equExpr.count(l) || textEqu.count(l) || labels.count(l);
    }

    void processLines(const std::vector<std::string>& lines, const std::string& fileName) {
        std::string savedFile = file;
        int savedLine = line;
        file = fileName;
        for (size_t idx = 0; idx < lines.size(); idx++) {
            line = static_cast<int>(idx) + 1;
            std::string s = trim(stripComment(lines[idx]));
            if (s.empty()) continue;
            if (s[0] == '%') s = trim(s.substr(1)); // % expansion operator prefix

            std::string w0, rest;
            {
                size_t sp = s.find_first_of(" \t");
                w0 = sp == std::string::npos ? s : s.substr(0, sp);
                rest = sp == std::string::npos ? "" : trim(s.substr(sp));
            }
            std::string l0 = lower(w0);

            // Conditionals are tracked even when inactive
            if (l0.rfind("if", 0) == 0 && (l0 == "if" || l0 == "ife" || l0 == "ifdef" || l0 == "ifndef" ||
                                           l0 == "ifidni" || l0 == "ifidn" || l0 == "ifdifi" || l0 == "ifdif" ||
                                           l0 == "ifb" || l0 == "ifnb")) {
                if (!active()) { ifStack.push_back(2); continue; }
                ifStack.push_back(evalCond(l0, rest) ? 1 : 0);
                continue;
            }
            if (l0.rfind("elseif", 0) == 0) {
                if (ifStack.empty()) { warn(where() + ": stray ELSEIF"); continue; }
                int& top = ifStack.back();
                if (top == 0) {
                    ifStack.pop_back();
                    bool outer = active();
                    ifStack.push_back(0);
                    if (outer) ifStack.back() = evalCond(l0.substr(4), rest) ? 1 : 0;
                } else {
                    top = 2;
                }
                continue;
            }
            if (l0 == "else") {
                if (ifStack.empty()) { warn(where() + ": stray ELSE"); continue; }
                int& top = ifStack.back();
                top = top == 0 ? 1 : 2;
                continue;
            }
            if (l0 == "endif") {
                if (ifStack.empty()) warn(where() + ": stray ENDIF");
                else ifStack.pop_back();
                continue;
            }
            if (!active()) {
                // Still skip macro/rept bodies so their ENDM doesn't confuse us
                std::string l1 = lower(firstWord(rest));
                if (l1 == "macro" || l0 == "rept" || l0 == "irp" || l0 == "irpc")
                    idx = skipBlock(lines, idx);
                continue;
            }

            std::string w1 = firstWord(rest);
            std::string l1 = lower(w1);
            std::string rest1 = trim(rest.substr(std::min(rest.size(), w1.size())));

            if (l0 == "include") { include(rest); continue; }
            if (l1 == "macro") {
                Macro m;
                for (auto& p : splitTop(rest1)) if (!p.empty()) m.params.push_back(lower(p));
                size_t end = skipBlock(lines, idx);
                for (size_t k = idx + 1; k < end; k++) m.body.push_back(lines[k]);
                macros[lower(w0)] = m;
                idx = end;
                continue;
            }
            if (l0 == "rept") {
                long long n = 0;
                try { n = evalNum(rest); } catch (const std::exception& ex) { warn(where() + ": rept " + ex.what()); }
                size_t end = skipBlock(lines, idx);
                std::vector<std::string> body(lines.begin() + idx + 1, lines.begin() + end);
                for (long long k = 0; k < n; k++) processLines(body, fileName + "(rept)");
                file = fileName;
                idx = end;
                continue;
            }
            if (l0 == "irp" || l0 == "irpc") { idx = skipBlock(lines, idx); continue; }

            if (l0 == ".dataseg" || (l1 == "segment" && lower(w0).find("data") != std::string::npos)) { inData = true; continue; }
            if (l0 == ".codeseg" || l0 == ".stackseg" || l0 == ".ends" || l0 == ".fardata" ||
                l1 == "ends" || (l1 == "segment")) { inData = false; continue; }

            if (l1 == "equ" || w1 == "=") {
                std::string name = lower(w0);
                std::string val = rest1;
                if (val.size() >= 2 && val.front() == '<' && val.back() == '>') {
                    textEqu[name] = val.substr(1, val.size() - 2);
                } else if (w1 == "=") {
                    // '=' symbols are redefinable: capture the value now
                    try {
                        Val v = eval(val);
                        equExpr[name] = v.known && v.codeRef.empty() ? std::to_string(v.v) : val;
                    } catch (const std::exception&) {
                        equExpr[name] = val;
                    }
                } else {
                    equExpr[name] = val;
                }
                continue;
            }

            if (l1 == "proc" || l0 == ".proc") {
                codeLabels.insert(lower(l0 == ".proc" ? firstWord(rest) : w0));
                continue;
            }
            if (!inData) {
                // Code label "name:" - remember so data refs to it are code refs
                if (w0.back() == ':') codeLabels.insert(lower(w0.substr(0, w0.size() - 1)));
                continue;
            }

            // ---- data segment statements ----
            if (l0 == "db" || l0 == "dw" || l0 == "dd") { dataDirective(l0, rest); continue; }
            if (l1 == "db" || l1 == "dw" || l1 == "dd") {
                defineLabel(w0);
                dataDirective(l1, rest1);
                continue;
            }
            if (l1 == "label") { defineLabel(w0); continue; }
            if (l0 == ".label") { defineLabel(firstWord(rest)); continue; }
            if (l0 == "even") { if (pc & 1) putBytes(0, 1, ""); continue; }
            if (l0 == "align") { long long a = evalNum(rest); while (a > 0 && (pc % a)) putBytes(0, 1, ""); continue; }
            if (w0.back() == ':') { defineLabel(w0.substr(0, w0.size() - 1)); continue; }
            if (l0 == "public" || l0 == ".public" || l0 == "extrn" || l0 == ".extrn" || l0 == "assume" ||
                l0 == "global" || l0 == ".global" || l0 == "title" || l0 == "page" || l0 == "name") continue;
            if (macros.count(l0)) { expandMacro(l0, rest); continue; }
            if (macros.count(l1)) { defineLabel(w0); expandMacro(l1, rest1); continue; }
            warn(where() + ": unhandled data statement: " + s);
        }
        file = savedFile;
        line = savedLine;
    }

    static std::string firstWord(const std::string& s) {
        size_t sp = s.find_first_of(" \t");
        return sp == std::string::npos ? s : s.substr(0, sp);
    }

    // Returns the index of the matching ENDM for a MACRO/REPT/IRP starting at idx
    size_t skipBlock(const std::vector<std::string>& lines, size_t idx) {
        int depth = 0;
        for (size_t k = idx; k < lines.size(); k++) {
            std::string s = lower(trim(stripComment(lines[k])));
            if (s.empty()) continue;
            std::string a = firstWord(s);
            std::string rest = trim(s.substr(a.size()));
            std::string b = firstWord(rest);
            if (b == "macro" || a == "rept" || a == "irp" || a == "irpc") depth++;
            else if (a == "endm") { if (--depth == 0) return k; }
        }
        warn(where() + ": unterminated block");
        return lines.size() - 1;
    }

    void expandMacro(const std::string& name, const std::string& args) {
        const Macro& m = macros[name];
        auto argv = splitTop(args);
        std::vector<std::string> body;
        for (std::string ln : m.body) {
            std::string c = trim(stripComment(ln));
            if (lower(firstWord(c)) == "local") continue;
            for (size_t p = 0; p < m.params.size(); p++) {
                std::string val = p < argv.size() ? argv[p] : "";
                // replace whole-word occurrences (case-insensitive), and &param&
                std::string out;
                size_t i = 0;
                while (i < ln.size()) {
                    if (isIdStart(ln[i])) {
                        size_t j = i;
                        while (j < ln.size() && isIdChar(ln[j])) j++;
                        std::string w = ln.substr(i, j - i);
                        out += lower(w) == m.params[p] ? val : w;
                        i = j;
                    } else if (ln[i] == '&') {
                        i++;
                    } else {
                        out += ln[i++];
                    }
                }
                ln = out;
            }
            body.push_back(ln);
        }
        processLines(body, file + "(" + name + ")");
    }

    void defineLabel(const std::string& raw) {
        std::string n = lower(raw);
        if (!emit) {
            if (labels.count(n)) warn(where() + ": duplicate label " + n);
            labels[n] = pc;
        } else if (labels[n] != pc) {
            warn(where() + ": label " + n + " moved between passes");
        }
    }

    void dataDirective(const std::string& dir, const std::string& items) {
        int size = dir == "db" ? 1 : dir == "dw" ? 2 : 4;
        try {
            emitItems(items, size);
        } catch (const std::exception& ex) {
            warn(where() + ": " + ex.what() + " in: " + items);
        }
    }

    std::vector<std::string> readFile(const std::string& fname) {
        std::string path = srcdir + "/" + fname;
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            // Case-insensitive fallback for DOS-style names
            path = srcdir + "/" + lower(fname);
            f.open(path, std::ios::binary);
        }
        std::vector<std::string> out;
        if (!f) return out;
        std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // DOS files: CRLF, sometimes bare CR, and a trailing ^Z
        std::string cur;
        for (size_t i = 0; i < all.size(); i++) {
            char c = all[i];
            if (c == '\x1a') break;
            if (c == '\r' || c == '\n') {
                out.push_back(cur);
                cur.clear();
                if (c == '\r' && i + 1 < all.size() && all[i + 1] == '\n') i++;
                continue;
            }
            cur += c;
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    void include(std::string fname) {
        fname = trim(fname);
        // Strip DOS paths like c:\sa\model.inc / c:\sa\include\imath.inc
        size_t slash = fname.find_last_of("\\/:");
        if (slash != std::string::npos) fname = fname.substr(slash + 1);
        auto lines = readFile(fname);
        if (lines.empty()) { warn(where() + ": include not found: " + fname); return; }
        processLines(lines, fname);
    }

    void run(bool pass2) {
        emit = pass2;
        pc = base;
        inData = false;
        ifStack.clear();
        evaluating.clear();
        if (!pass2) {
            labels.clear();
            equExpr.clear();
            textEqu.clear();
            macros.clear();
        }
        auto lines = readFile(name);
        if (lines.empty()) throw std::runtime_error("cannot read " + srcdir + "/" + name);
        processLines(lines, name);
        if (!ifStack.empty()) warn(name + ": unbalanced IF at end of file");
    }
};

bool validIdent(const std::string& s) {
    static const std::set<std::string> reserved = {
        "and", "or", "not", "xor", "int", "char", "short", "long", "float", "double", "void", "auto",
        "delete", "new", "this", "true", "false", "case", "default", "do", "for", "if", "else", "while",
        "switch", "break", "continue", "return", "goto", "const", "static", "struct", "class", "union",
        "enum", "signed", "unsigned", "register", "extern", "volatile", "inline", "try", "catch",
        "throw", "operator", "template", "typename", "public", "private", "protected", "friend",
        "virtual", "namespace", "using", "sizeof", "typedef", "asm", "bool", "export", "mutable",
        "explicit", "wchar_t", "near", "far", "small", "large", "huge", "min", "max", "errno",
        "stdin", "stdout", "stderr", "assert"};
    if (s.empty() || std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    for (char c : s) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    if (s[0] == '_' && s.size() > 1 && (s[1] == '_' || std::isupper(static_cast<unsigned char>(s[1])))) return false;
    return !reserved.count(s);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: asmdata <srcdir> <out.cpp> <out.hpp> <module[:DEF=v,...]>...\n");
        return 2;
    }
    std::string srcdir = argv[1], outCpp = argv[2], outHpp = argv[3];
    Global g;
    std::vector<Module> mods;
    for (int i = 4; i < argc; i++) {
        Module m;
        std::string spec = argv[i];
        size_t colon = spec.find(':');
        m.name = spec.substr(0, colon);
        if (colon != std::string::npos) {
            for (auto& d : splitTop(spec.substr(colon + 1))) {
                size_t eq = d.find('=');
                m.defines[lower(d.substr(0, eq))] = eq == std::string::npos ? 1 : std::strtoll(d.c_str() + eq + 1, nullptr, 0);
            }
        }
        m.srcdir = srcdir;
        m.g = &g;
        mods.push_back(m);
    }

    std::vector<uint8_t> image;
    std::vector<CodeRef> codeRefs;
    try {
        // Pass 1: sizes and labels, module by module in link order
        long long base = 0;
        for (auto& m : mods) {
            m.image = &image;
            m.codeRefs = &codeRefs;
            m.base = base;
            m.run(false);
            base = (m.pc + 1) & ~1LL; // word-aligned segment combine
            for (auto& [n, off] : m.labels) {
                if (!g.publicLabels.count(n)) { g.publicLabels[n] = off; g.labelModule[n] = m.name; }
            }
        }
        // Pass 2: emit with every label known
        g.finalPass = true;
        for (auto& m : mods) m.run(true);
        image.resize(static_cast<size_t>(base), 0);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "asmdata: fatal: %s\n", ex.what());
        return 1;
    }

    // Drop code refs that are really numeric constants of 0 (none), keep names unique per offset
    std::sort(codeRefs.begin(), codeRefs.end(), [](const CodeRef& a, const CodeRef& b) { return a.offset < b.offset; });

    std::ofstream h(outHpp), c(outCpp);
    if (!h || !c) { std::fprintf(stderr, "asmdata: cannot write outputs\n"); return 1; }

    h << "// Generated by tools/asmdata from the original MASM sources - do not edit.\n"
         "#pragma once\n#include <cstddef>\n#include <cstdint>\n\nnamespace corncob::dgroup {\n\n"
         "extern const uint8_t kImage[];\nconstexpr size_t kImageSize = " << image.size() << ";\n\n"
         "struct Symbol { const char* module; const char* name; uint32_t offset; };\n"
         "extern const Symbol kSymbols[];\nextern const size_t kSymbolCount;\n\n"
         "// Data words that hold the address of a code label (jump tables, routine pointers)\n"
         "struct CodeRef { uint32_t offset; uint8_t size; const char* name; };\n"
         "extern const CodeRef kCodeRefs[];\nextern const size_t kCodeRefCount;\n\n"
         "// Per-module label offsets, e.g. dgroup::m3::ox, dgroup::defobjs::plnplyobj\n";
    for (auto& m : mods) {
        std::string ns = lower(m.name.substr(0, m.name.find('.')));
        if (std::isdigit(static_cast<unsigned char>(ns[0]))) ns = "m" + ns;
        h << "namespace " << ns << " {\n";
        for (auto& [n, off] : m.labels)
            if (validIdent(n)) h << "constexpr uint32_t " << n << " = 0x" << std::hex << off << std::dec << ";\n";
        h << "} // namespace " << ns << "\n\n";
    }
    h << "// Numeric EQU constants per module (evaluated), e.g. dgroup::defobjs_equ::lp\n";
    for (auto& m : mods) {
        std::string ns = lower(m.name.substr(0, m.name.find('.')));
        if (std::isdigit(static_cast<unsigned char>(ns[0]))) ns = "m" + ns;
        h << "namespace " << ns << "_equ {\n";
        for (auto& [n, e] : m.equExpr) {
            if (!validIdent(n) || m.labels.count(n)) continue;
            try {
                auto v = m.eval(e);
                if (!v.known || !v.codeRef.empty()) continue;
                h << "constexpr long " << n << " = " << v.v << ";\n";
            } catch (...) {
            }
        }
        h << "} // namespace " << ns << "_equ\n\n";
    }
    h << "} // namespace corncob::dgroup\n\n";

    // Flat view for code that does not care which module a label comes from: every label that is
    // defined in exactly one module, e.g. lbl::ox. Names defined in several modules (kbuf, exptr ...)
    // are left out; use the per-module namespaces for those.
    {
        std::map<std::string, int> count;
        for (auto& m : mods)
            for (auto& [n, off] : m.labels) count[n]++;
        h << "namespace corncob::lbl {\n";
        for (auto& m : mods)
            for (auto& [n, off] : m.labels)
                if (count[n] == 1 && validIdent(n))
                    h << "constexpr uint32_t " << n << " = 0x" << std::hex << off << std::dec << ";\n";
        h << "} // namespace corncob::lbl\n";
    }

    c << "// Generated by tools/asmdata from the original MASM sources - do not edit.\n"
         "#include \"DGroup.hpp\"\n\nnamespace corncob::dgroup {\n\nconst uint8_t kImage[] = {";
    for (size_t i = 0; i < image.size(); i++) {
        if (i % 24 == 0) c << "\n";
        c << static_cast<int>(image[i]) << ",";
    }
    c << "\n};\n\nconst Symbol kSymbols[] = {\n";
    size_t nsym = 0;
    for (auto& m : mods)
        for (auto& [n, off] : m.labels) {
            c << "    {\"" << m.name << "\", \"" << n << "\", " << off << "},\n";
            nsym++;
        }
    c << "};\nconst size_t kSymbolCount = " << nsym << ";\n\nconst CodeRef kCodeRefs[] = {\n";
    for (auto& r : codeRefs) c << "    {" << r.offset << ", " << r.size << ", \"" << lower(r.name) << "\"},\n";
    if (codeRefs.empty()) c << "    {0, 0, nullptr},\n";
    c << "};\nconst size_t kCodeRefCount = " << codeRefs.size() << ";\n\n} // namespace corncob::dgroup\n";

    std::fprintf(stderr, "asmdata: %zu modules, %zu bytes of data, %zu symbols, %zu code refs, %d warnings\n",
                 mods.size(), image.size(), nsym, codeRefs.size(), warn.count);
    return 0;
}
