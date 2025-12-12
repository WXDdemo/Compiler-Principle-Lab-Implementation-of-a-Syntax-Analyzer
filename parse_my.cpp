#include <bits/stdc++.h>
#include <iomanip>
using namespace std;

bool isNonTerminal(char c) {
    return isupper(static_cast<unsigned char>(c));
}

class Grammar {
private:
    set<char> Vn;
    set<char> Vt;
    char S;
    map<char, set<string> > P;
    map<char, set<char> > FIRST;
    map<char, set<char> > FOLLOW;
    map<string, string> Table;

    // 将 source 中（可能含 @）合并到 target，根据 includeEpsilon 决定是否包含 '@'
    void addToSet(set<char>& target, const set<char>& source, bool includeEpsilon) {
        for (char c : source) {
            if (c != '@' || includeEpsilon) {
                target.insert(c);
            }
        }
    }

public:
    Grammar(string filename) {
        Vn.clear();
        Vt.clear();
        P.clear();
        FIRST.clear();
        FOLLOW.clear();
        Table.clear();

        ifstream in(filename.c_str());
        if (!in.is_open()) {
            cout << "文法文件打开失败: " << filename << endl;
            exit(1);
        }

        // 读取整个文件（保留分号 ; 作为产生式分隔）
        string content;
        string line;
        while (getline(in, line)) {
            content += line;
            content += '\n';
        }
        in.close();

        // 以分号 ; 分割产生式组
        stringstream ss(content);
        string rule;
        bool firstRule = true;

        while (getline(ss, rule, ';')) {
            // 去除空白字符
            rule.erase(remove_if(rule.begin(), rule.end(), [](char c){ return isspace(static_cast<unsigned char>(c)); }), rule.end());
            if (rule.empty()) continue;

            size_t arrowPos = rule.find("->");
            if (arrowPos == string::npos) continue;

            // 左部可以是多个字符，但我们假定单字符非终结符（如 E）
            string leftStr = rule.substr(0, arrowPos);
            if (leftStr.empty()) continue;
            char left = leftStr[0];

            string right = rule.substr(arrowPos + 2);
            if (right.empty()) continue;

            Vn.insert(left);
            if (firstRule) {
                S = left;
                firstRule = false;
            }

            // 右部用 | 分隔多个产生式
            stringstream ss_right(right);
            string production;
            while (getline(ss_right, production, '|')) {
                if (production.empty()) continue;
                // production 可能是 "@" 表示 ε
                P[left].insert(production);

                // 收集终结符（不是大写字母且非 @）
                for (char c : production) {
                    if (!isNonTerminal(c) && c != '@') {
                        Vt.insert(c);
                    }
                }
            }
        }

        // 把输入结束符 '#' 作为终结符
        Vt.insert('#');

        // 为所有非终结符在 FIRST/FOLLOW 中创建空集（便于后续访问）
        for (char A : Vn) {
            FIRST[A] = set<char>();
            FOLLOW[A] = set<char>();
        }
    }

    void setStartSymbol(char start) {
        S = start;
    }

    char getStartSymbol() {
        return S;
    }

    void print() {
        cout << "文法规则：" << endl;
        // 按字母序打印
        vector<char> sortedVn(Vn.begin(), Vn.end());
        sort(sortedVn.begin(), sortedVn.end());
        for (char c : sortedVn) {
            cout << c << "->";
            bool first = true;
            // 保证产生式顺序稳定（使用 vector 从 set 复制会自然有序）
            vector<string> prods(P[c].begin(), P[c].end());
            for (const string& prod : prods) {
                if (!first) cout << "|";
                cout << prod;
                first = false;
            }
            cout << endl;
        }
    }

    // 计算所有非终结符的 FIRST
    void computeFirst() {
        // 初始化 FIRST（确保所有非终结符键存在）
        for (char A : Vn) FIRST[A] = set<char>();

        bool changed = true;
        while (changed) {
            changed = false;
            for (char A : Vn) {
                for (const string& alpha : P[A]) {
                    int oldSize = FIRST[A].size();

                    if (alpha == "@") {
                        FIRST[A].insert('@');
                    } else {
                        bool allNullable = true;
                        // 逐个符号看能否产生终结符或 epsilon
                        for (size_t i = 0; i < alpha.size(); ++i) {
                            char symbol = alpha[i];
                            if (!isNonTerminal(symbol)) {
                                // 终结符直接加入 FIRST(A)
                                FIRST[A].insert(symbol);
                                allNullable = false;
                                break;
                            } else {
                                // 将 FIRST(symbol) 中除 @ 之外的符号加入 FIRST(A)
                                addToSet(FIRST[A], FIRST[symbol], false);
                                if (FIRST[symbol].find('@') == FIRST[symbol].end()) {
                                    allNullable = false;
                                    break;
                                }
                                // 否则继续看下一个符号
                            }
                        }
                        if (allNullable) {
                            FIRST[A].insert('@');
                        }
                    }

                    if (FIRST[A].size() > oldSize) changed = true;
                }
            }
        }
    }

    // 计算字符串的 FIRST（用于 FOLLOW 及构建预测表）
    set<char> computeFirstOfString(const string& str) {
        set<char> result;
        if (str.empty()) {
            result.insert('@');
            return result;
        }

        bool allNullable = true;
        for (size_t i = 0; i < str.size(); ++i) {
            char symbol = str[i];
            if (symbol == '@') {
                // '@' 表示空串 -> 继续（但通常不会出现在中间）
                continue;
            }
            if (!isNonTerminal(symbol)) {
                result.insert(symbol);
                allNullable = false;
                break;
            } else {
                // symbol 是非终结符
                for (char c : FIRST[symbol]) {
                    if (c != '@') result.insert(c);
                }
                if (FIRST[symbol].find('@') == FIRST[symbol].end()) {
                    allNullable = false;
                    break;
                }
            }
        }

        if (allNullable) result.insert('@');
        return result;
    }

    void computeFollow() {
        // 初始化 FOLLOW 键（已在构造时创建，但再次确保）
        for (char A : Vn) FOLLOW[A]; // no-op, ensure entry exists
        FOLLOW[S].insert('#');

        bool changed = true;
        while (changed) {
            changed = false;
            for (char A : Vn) {
                for (const string& alpha : P[A]) {
                    for (size_t i = 0; i < alpha.size(); ++i) {
                        char B = alpha[i];
                        if (!isNonTerminal(B)) continue;

                        int oldSize = FOLLOW[B].size();

                        if (i + 1 < alpha.size()) {
                            string beta = alpha.substr(i + 1);
                            set<char> firstBeta = computeFirstOfString(beta);
                            // 把 firstBeta (除 @) 加入 FOLLOW(B)
                            for (char b : firstBeta) {
                                if (b != '@') FOLLOW[B].insert(b);
                            }
                            // 若 beta 可以推出 ε，则把 FOLLOW(A) 加入 FOLLOW(B)
                            if (firstBeta.find('@') != firstBeta.end()) {
                                addToSet(FOLLOW[B], FOLLOW[A], true);
                            }
                        } else {
                            // B 在最右侧，把 FOLLOW(A) 加入 FOLLOW(B)
                            addToSet(FOLLOW[B], FOLLOW[A], true);
                        }

                        if (FOLLOW[B].size() > oldSize) changed = true;
                    }
                }
            }
        }
    }

    // 构建 LL(1) 分析表
    void buildTable() {
        Table.clear();
        // 终结符集合（不包含 @）
        set<char> terminals = Vt;
        terminals.insert('#');

        for (char A : Vn) {
            for (const string& alpha : P[A]) {
                set<char> firstAlpha = computeFirstOfString(alpha);
                for (char a : firstAlpha) {
                    if (a != '@') {
                        string key; key.push_back(A); key.push_back(a);
                        Table[key] = alpha;
                    }
                }
                if (firstAlpha.find('@') != firstAlpha.end()) {
                    // 对 FOLLOW(A) 中的每个 b，M[A,b] = alpha
                    for (char b : FOLLOW[A]) {
                        string key; key.push_back(A); key.push_back(b);
                        Table[key] = alpha;
                    }
                }
            }
        }

        // 填充 error 项（使后续查表不会产生未定义）
        for (char A : Vn) {
            for (char a : terminals) {
                string key; key.push_back(A); key.push_back(a);
                if (Table.find(key) == Table.end()) Table[key] = "error";
            }
        }
    }

    void printFirstSets() {
        cout << "FIRST集为：" << endl;
        vector<char> sortedVn(Vn.begin(), Vn.end());
        sort(sortedVn.begin(), sortedVn.end());
        for (char c : sortedVn) {
            cout << "FIRST(" << c << ") = {";
            bool first = true;
            vector<char> elems(FIRST[c].begin(), FIRST[c].end());
            sort(elems.begin(), elems.end());
            for (char ch : elems) {
                if (!first) cout << ", ";
                cout << ch;
                first = false;
            }
            cout << "}" << endl;
        }
    }

    void printFollowSets() {
        cout << "FOLLOW集为：" << endl;
        vector<char> sortedVn(Vn.begin(), Vn.end());
        sort(sortedVn.begin(), sortedVn.end());
        for (char c : sortedVn) {
            cout << "FOLLOW(" << c << ") = {";
            bool first = true;
            vector<char> elems(FOLLOW[c].begin(), FOLLOW[c].end());
            sort(elems.begin(), elems.end());
            for (char ch : elems) {
                if (!first) cout << ", ";
                cout << ch;
                first = false;
            }
            cout << "}" << endl;
        }
    }

    void printTable() {
        cout << "LL(1)分析表：" << endl << endl;

        vector<char> sortedVn(Vn.begin(), Vn.end());
        sort(sortedVn.begin(), sortedVn.end());

        set<char> terminals = Vt;
        terminals.insert('#');
        vector<char> sortedVt(terminals.begin(), terminals.end());
        sort(sortedVt.begin(), sortedVt.end());

        cout << setw(8) << " ";
        for (char t : sortedVt) {
            cout << setw(12) << t;
        }
        cout << endl << string(12 * (sortedVt.size() + 1), '-') << endl;

        for (char nt : sortedVn) {
            cout << setw(7) << nt;
            for (char t : sortedVt) {
                string key; key.push_back(nt); key.push_back(t);
                string value = (Table.find(key) != Table.end()) ? Table[key] : "error";
                if (value == "error") {
                    cout << setw(12) << " ";
                } else {
                    string cell = string(1, nt) + "->" + value;
                    cout << setw(12) << cell;
                }
            }
            cout << endl;
        }
    }

    // 消除左递归（间接 + 直接）
    void removeLeftRecursion() {
        // 以一个快照顺序遍历非终结符（按字母序）
        vector<char> nonTerminals(Vn.begin(), Vn.end());
        sort(nonTerminals.begin(), nonTerminals.end());

        for (int i = 0; i < (int)nonTerminals.size(); ++i) {
            char Ai = nonTerminals[i];

            // 消除间接左递归：对于 Aj (j < i)，把以 Aj 开头的 Ai->Ajγ 替换为 Aj 的产生式的右部拼接 γ
            for (int j = 0; j < i; ++j) {
                char Aj = nonTerminals[j];
                set<string> newProductions;
                for (const string& prod : P[Ai]) {
                    if (!prod.empty() && prod[0] == Aj) {
                        string suffix = prod.substr(1); // γ
                        for (const string& beta : P[Aj]) {
                            if (beta == "@") {
                                // Aj -> ε, 则 Ai 的产生式变成 γ（即 suffix）
                                if (suffix.empty()) newProductions.insert("@");
                                else newProductions.insert(suffix);
                            } else {
                                // 把 beta(可能为终结符串或非终结符串) 与 suffix 拼接
                                string combined = beta;
                                if (suffix != "@") combined += suffix;
                                newProductions.insert(combined);
                            }
                        }
                    } else {
                        newProductions.insert(prod);
                    }
                }
                P[Ai] = newProductions;
            }

            // 消除 Ai 的直接左递归
            eliminateDirectLeftRecursion(Ai);

            // 注意：如果在 eliminate 中新增了新的非终结符（A'），它不会出现在 nonTerminals 中的当前快照里。
            // 这是符合标准算法（新非终结符在后续不需要再次处理间接左递归）。
        }
    }

    // 只处理 A 的直接左递归
    void eliminateDirectLeftRecursion(char A) {
        vector<string> alpha; // A -> A α
        vector<string> beta;  // A -> β (β 不以 A 开头)

        for (const string& prod : P[A]) {
            if (!prod.empty() && prod[0] == A) {
                string suf = prod.substr(1);
                if (suf.empty()) suf = "@";
                alpha.push_back(suf);
            } else {
                if (prod.empty()) beta.push_back("@");
                else beta.push_back(prod);
            }
        }

        if (alpha.empty()) return; // 没有直接左递归

        // 找一个新的非终结符（大写）作为 A'
        char newNonTerminal = 0;
        for (char c = 'A'; c <= 'Z'; ++c) {
            if (Vn.find(c) == Vn.end() && c != A) {
                newNonTerminal = c;
                break;
            }
        }
        if (newNonTerminal == 0) {
            // 如果没有可用的大写字母，选择一个小写开头的伪非终结符（不推荐，但兜底）
            for (char c = 'a'; c <= 'z'; ++c) {
                if (Vn.find(c) == Vn.end()) {
                    newNonTerminal = c;
                    break;
                }
            }
        }
        if (newNonTerminal == 0) {
            cout << "无法创建新的非终结符来消除左递归（非终结符用尽）" << endl;
            exit(1);
        }

        Vn.insert(newNonTerminal);
        // 为 FIRST/FOLLOW 初始化
        FIRST[newNonTerminal] = set<char>();
        FOLLOW[newNonTerminal] = set<char>();

        // A -> β A'
        set<string> newAprods;
        for (const string& b : beta) {
            if (b == "@") {
                // β 为 ε，则 A -> A'
                string prod;
                prod.push_back(newNonTerminal);
                newAprods.insert(prod);
            } else {
                string prod = b;
                prod.push_back(newNonTerminal);
                newAprods.insert(prod);
            }
        }
        P[A] = newAprods;

        // A' -> α A' | ε
        set<string> newAprime;
        for (const string& a : alpha) {
            if (a == "@") {
                // 原来的 A -> Aα 中 α 为 ε，A' -> A'
                string prod;
                prod.push_back(newNonTerminal);
                newAprime.insert(prod);
            } else {
                string prod = a;
                prod.push_back(newNonTerminal);
                newAprime.insert(prod);
            }
        }
        newAprime.insert("@"); // ε
        P[newNonTerminal] = newAprime;
        // 注意：不要把 '@' 放入终结符集合 Vt（@ 是 ε）
    }

    void printAll() {
        cout << "\n文法信息：" << endl;
        cout << "非终结符: ";
        vector<char> vn(Vn.begin(), Vn.end());
        sort(vn.begin(), vn.end());
        for (char c : vn) cout << c << " ";
        cout << "\n终结符: ";
        vector<char> vt(Vt.begin(), Vt.end());
        sort(vt.begin(), vt.end());
        for (char c : vt) cout << c << " ";
        cout << "\n开始符号: " << S << endl;

        print();
    }

    // LL(1) 分析（输入串最后必须有 #）
    bool analyze(const string& input) {
        stack<char> stk;
        stk.push('#');
        stk.push(S);

        int pos = 0;
        if (input.empty()) {
            cout << "输入为空！" << endl;
            return false;
        }
        char a = input[pos];

        cout << "\n开始分析：" << endl;
        cout << setw(20) << left << "符号栈"
             << setw(20) << left << "输入串"
             << setw(20) << left << "动作" << endl;
        cout << string(60, '-') << endl;

        while (true) {
            // 打印当前栈（从栈底到栈顶）
            string stackStr = "";
            stack<char> temp = stk;
            vector<char> vec;
            while (!temp.empty()) {
                vec.push_back(temp.top());
                temp.pop();
            }
            reverse(vec.begin(), vec.end());
            for (char c : vec) stackStr.push_back(c);

            string inputStr = (pos < (int)input.size()) ? input.substr(pos) : string("#");

            cout << setw(20) << left << stackStr
                 << setw(20) << left << inputStr;

            char X = stk.top();
            stk.pop();

            if (X == '#') {
                if (a == '#') {
                    cout << setw(20) << left << "接受" << endl;
                    return true;
                } else {
                    cout << setw(20) << left << "错误: 栈已空但输入未结束" << endl;
                    return false;
                }
            }

            if (!isNonTerminal(X)) {
                if (X == a) {
                    cout << setw(20) << left << ("匹配 " + string(1, X)) << endl;
                    pos++;
                    if (pos < (int)input.size()) a = input[pos];
                    else a = '#';
                } else {
                    cout << setw(20) << left << "错误: 终结符不匹配" << endl;
                    return false;
                }
            } else {
                string key; key.push_back(X); key.push_back(a);
                if (Table.find(key) == Table.end() || Table[key] == "error") {
                    cout << setw(20) << left << "错误: 无产生式可用" << endl;
                    return false;
                }
                string production = Table[key];
                cout << setw(20) << left << (string(1, X) + "->" + production) << endl;
                if (production != "@") {
                    // 倒序把产生式右部压栈
                    for (int i = (int)production.length() - 1; i >= 0; --i) {
                        stk.push(production[i]);
                    }
                }
            }
        }

        return false;
    }
};

int main() {
    string filename;
    cout << "请输入文法文件名: ";
    cin >> filename;

    Grammar grammar(filename);

    cout << "\n========== 原始文法 ==========" << endl;
    grammar.printAll();

    cout << "\n========== 消除左递归 ==========" << endl;
    Grammar grammarNoLR = grammar;  // 复制一份
    grammarNoLR.removeLeftRecursion();
    grammarNoLR.print();

    cout << "\n========== 计算FIRST和FOLLOW集 ==========" << endl;
    grammarNoLR.computeFirst();
    grammarNoLR.printFirstSets();
    cout << endl;
    grammarNoLR.computeFollow();
    grammarNoLR.printFollowSets();

    cout << "\n========== 构建LL(1)分析表 ==========" << endl;
    grammarNoLR.buildTable();
    grammarNoLR.printTable();

    string input;
    cout << "\n请输入要分析的字符串（以#结束，例如 i+i#）: ";
    cin >> input;

    cout << "\n========== 分析输入串: " << input << " ==========" << endl;
    if (grammarNoLR.analyze(input)) {
        cout << "\n分析成功！输入串属于该文法。" << endl;
    } else {
        cout << "\n分析失败！输入串不属于该文法。" << endl;
    }

    return 0;
}
