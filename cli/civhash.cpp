// civhash.cpp — CivNexus6 hash CLI
// 提取自 ConsoleApplication1/ConsoleApplication1.cpp：
//   - crc32_table / calculate_crc32 / ComputeHash 逐字保留（Civ 风格 CRC32，ComputeHash 返回原始状态、无最终异或）
//   - 骨骼层级哈希：等价于原 calculate_bone_hierarchy_hash（沿父链自叶向根累加 CRC，再累加轨道名，最后 ^0xFFFFFFFF）
//   - reverse：已知前缀 + 目标 CRC32，暴力反推剩余后缀（原 reverse_crc32 为实验性代码，这里改为可靠的可控字符集搜索）
//
// 用法：
//   civhash <text>                                  计算 Civ 风格哈希（raw + 标准 CRC32）
//   civhash <track> --bone Root/BoneA/BoneB         骨骼链哈希（路径自根到叶，末位为目标骨骼）
//   civhash --reverse <crc32> [--prefix P] [--max N] [--charset S]   反推后缀
//   通用： --json 输出 JSON；-h 帮助
//
// 编译： g++ -std=c++17 -O2 -fwrapv -static -s -o civhash.exe civhash.cpp

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// ===== 以下 CRC32 查表逐字复制自原工程 ConsoleApplication1.cpp =====
const uint32_t crc32_table[] =
{
    0,0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3, 0x0EDB8832,
0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064,
0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856,
0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8,
0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA,
0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC,
0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F, 0x2802B89E,
0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190,
0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2,
0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4,
0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6,
0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158,
0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A,
0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C,
0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F, 0x5EDEF90E,
0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320,
0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12,
0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1, 0xF00F9344,
0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76,
0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8,
0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B, 0xD80D2BDA,
0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79, 0xCB61B38C,
0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE,
0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0,
0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713, 0x95BF4A82,
0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4,
0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777, 0x88085AE6,
0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45, 0xA00AE278,
0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A,
0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C,
0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E,
0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D, 0xEDB88320,
0xDEBB20E3
};

uint32_t calculate_crc32(const char* data, size_t length, uint32_t crc = 0xFFFFFFFF) {
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        crc = (crc >> 8) ^ crc32_table[(crc ^ byte) & 0xFF];
    }
    return crc;
}

uint32_t ComputeHash(const char* text) {
    const char* ptr = text;
    uint32_t hash = static_cast<uint32_t>(-1);  // 初始值 0xFFFFFFFF

    while (*ptr != '\0') {
        uint8_t current_char = *ptr;
        uint32_t table_index = (hash ^ current_char) & 0xFF;  // 取低 8 位
        hash = (hash >> 8) ^ crc32_table[table_index];
        ptr++;
    }

    return hash;
}
// ===== 原工程代码复制结束 =====

// ---- FNV-1a 32 位（引擎反编译实证：Platform::strhash @ exe+0x982610、
//      String::StaticHash_FNV1A::Hash @ exe+0x92BD0）----
// init = 0x811C9DC5, prime = 0x01000193, 每字节 h = (h ^ b) * prime（先异或后乘，是 FNV-1a 不是 FNV-1）
static const uint32_t FNV_DEFAULT_SEED = 0x811C9DC5u;

static inline uint32_t fnv_step(uint32_t h, uint32_t b) { return (h ^ b) * 0x01000193u; }

// fold=true：'A'..'Z' 先 +32 转小写再异或（strhash 第三参 fold 分支；String::Hash 不走此分支）。
// 字节 >= 0x80 按引擎 MSVC 有符号 char 符号扩展处理（等价 h ^= 0xFFFFFF00|b），与引擎逐位一致。
static uint32_t fnv1a32(const char* s, size_t len, uint32_t seed, bool fold) {
    uint32_t h = seed;
    for (size_t i = 0; i < len; ++i) {
        uint32_t b = static_cast<uint8_t>(s[i]);
        if (fold && b >= 'A' && b <= 'Z') b += 32;
        if (b >= 0x80) b |= 0xFFFFFF00u;
        h = fnv_step(h, b);
    }
    return h;
}

enum Algo { ALGO_CRC, ALGO_FNV };

static inline uint32_t crc_step(uint32_t s, uint8_t c) {
    return (s >> 8) ^ crc32_table[(s ^ c) & 0xFF];
}
static inline uint32_t step_state(uint32_t s, uint8_t c, Algo algo) {
    return algo == ALGO_FNV ? fnv_step(s, c) : crc_step(s, c);
}

// 骨骼层级哈希：path 为 自根到叶 的骨骼名（等价原 calculate_bone_hierarchy_hash 沿父链自叶向根累加）
// 返回值语义与原函数一致：链上名字 + trackName 的链式 CRC 后 ^0xFFFFFFFF
static bool bone_hierarchy_hash(const std::vector<std::string>& path, const std::string& track, uint32_t& out) {
    if (path.empty()) return false;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = path.size(); i-- > 0; )            // 叶 -> 根
        crc = calculate_crc32(path[i].c_str(), path[i].size(), crc);
    crc = calculate_crc32(track.c_str(), track.size(), crc);
    out = crc ^ 0xFFFFFFFF;
    return true;
}

// 骨骼层级哈希（FNV）：段间直接续状态（FNV 无终结步骤，等价于 叶->根 名字+track 拼接后单次 FNV）
static bool bone_hierarchy_hash_fnv(const std::vector<std::string>& path, const std::string& track,
                                    uint32_t seed, bool fold, uint32_t& out) {
    if (path.empty()) return false;
    uint32_t h = seed;
    for (size_t i = path.size(); i-- > 0; )            // 叶 -> 根
        h = fnv1a32(path[i].c_str(), path[i].size(), h, fold);
    out = fnv1a32(track.c_str(), track.size(), h, fold);
    return true;
}

// 反推（暴力 odometer，CRC/FNV 共用）：从 prefix_state 出发在 charset 上搜索长度 0..maxlen 的后缀，
// 状态命中 targets 中任一值即记录（多目标共用一次遍历）。总命中达 128 截断。
static void reverse_search(uint32_t prefix_state, const std::vector<uint32_t>& targets,
                           const std::string& charset, int maxlen, Algo algo,
                           bool& truncated, std::vector<std::vector<std::string>>& hits) {
    truncated = false;
    hits.assign(targets.size(), {});
    const int N = static_cast<int>(charset.size());
    if (N <= 0 || maxlen <= 0 || targets.empty()) return;

    size_t total = 0;
    for (size_t t = 0; t < targets.size(); ++t)
        if (prefix_state == targets[t]) { hits[t].push_back(""); ++total; }   // 长度 0：前缀本身即命中

    std::vector<int> idx;
    std::vector<uint32_t> st;                          // st[k] = 已消耗前 k 个后缀字符后的状态
    for (int L = 1; L <= maxlen && !truncated; ++L) {
        idx.assign(L, 0);
        st.assign(L + 1, prefix_state);
        for (int k = 0; k < L; ++k)
            st[k + 1] = step_state(st[k], static_cast<uint8_t>(charset[idx[k]]), algo);
        while (true) {
            for (size_t t = 0; t < targets.size(); ++t) {
                if (st[L] == targets[t]) {
                    std::string s;
                    for (int k = 0; k < L; ++k) s += charset[idx[k]];
                    hits[t].push_back(s);
                    if (++total >= 128) { truncated = true; break; }
                }
            }
            if (truncated) break;
            int k = L - 1;                              // odometer 自增
            while (k >= 0) { if (++idx[k] < N) break; idx[k] = 0; --k; }
            if (k < 0) break;
            uint32_t run = (k == 0) ? prefix_state : st[k];
            for (int j = k; j < L; ++j) {
                run = step_state(run, static_cast<uint8_t>(charset[idx[j]]), algo);
                st[j + 1] = run;
            }
        }
    }
}

// ---- 输出辅助 ----
static std::string json_escape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
            else o += static_cast<char>(c);
        }
    }
    return o;
}
static std::string hex32(uint32_t v) {
    char b[16]; std::snprintf(b, sizeof(b), "0x%08X", v); return b;
}
// 数值输出格式：默认 hex；--int 有符号 int32；--uint 无符号 uint32（--reverse 下忽略）
enum { FMT_HEX = 0, FMT_INT = 1, FMT_UINT = 2 };
static const char* fmt_name(int f) { return f == FMT_INT ? "int" : (f == FMT_UINT ? "uint" : "hex"); }
static std::string fmt_u32(uint32_t v, int f) {
    char b[32];
    if (f == FMT_INT) std::snprintf(b, sizeof(b), "%d", static_cast<int32_t>(v));
    else if (f == FMT_UINT) std::snprintf(b, sizeof(b), "%u", v);
    else std::snprintf(b, sizeof(b), "0x%08X", v);
    return b;
}
static std::string json_num32(uint32_t v, int f) {
    char b[32];
    if (f == FMT_INT) std::snprintf(b, sizeof(b), "%d", static_cast<int32_t>(v));
    else std::snprintf(b, sizeof(b), "%u", v);   // hex/uint 都按无符号输出数值字段
    return b;
}
static void print_usage() {
    std::cout <<
        "civhash - CivNexus6 hash CLI (extracted from ConsoleApplication1)\n"
        "\n"
        "用法:\n"
        "  civhash <text>                                    Civ 风格 CRC32 哈希 (raw + 标准)\n"
        "  civhash --fnv <text>                              引擎 FNV-1a 哈希\n"
        "  civhash <track> --bone Root/BoneA/BoneB           骨骼链哈希, 路径自根到叶, 末位为目标骨骼\n"
        "  civhash --reverse <target>[,<target>...] [opts]   反推后缀 (逗号分隔多目标批量)\n"
        "      --prefix <P>       已知字段前缀 (默认空)\n"
        "      --max <N>          后缀最大长度 1..6 (默认 5; 大字符集 max6 需数十秒)\n"
        "      --charset <S>      搜索字符集 (默认 A-Z 0-9 _)\n"
        "  算法选择:\n"
        "      --fnv              使用引擎 FNV-1a (Platform::strhash / StaticHash_FNV1A)\n"
        "                         开启后 --reverse 也按 FNV 反推, --bone 同样切换算法\n"
        "      --seed <N>         FNV 种子 (默认 0x811C9DC5, 仅 --fnv 生效)\n"
        "      --fold             FNV 大小写折叠变体 ('A'..'Z'->+32 后再异或, 仅 --fnv)\n"
        "  数值格式: 默认 hex; --int 有符号 int32 十进制; --uint 无符号 uint32 十进制\n"
        "           (仅 hash/bone 模式生效, --reverse 下自动忽略)\n"
        "  通用: --json 输出 JSON; -h/--help 帮助\n"
        "\n"
        "说明: CRC raw = ComputeHash 原语义 (无最终异或), 标准 = raw ^ 0xFFFFFFFF;\n"
        "      FNV-1a: h=(h^b)*0x01000193, init=seed(默认 0x811C9DC5), 返回完整 u32;\n"
        "      字节 >=0x80 按引擎 MSVC 有符号 char 符号扩展处理 (等价 b|0xFFFFFF00);\n"
        "      目标值接受 0x 十六进制或十进制\n";
}

int main(int argc, char** argv) {
    std::string text, bone, prefix, rev_str, charset_str;
    bool json = false, have_rev = false, fold = false;
    int maxlen = 5;
    int fmt = FMT_HEX;
    Algo algo = ALGO_CRC;
    uint32_t seed = FNV_DEFAULT_SEED;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { std::cerr << "error: " << name << " 缺少参数值" << std::endl; std::exit(2); }
            return argv[++i];
        };
        if (a == "--json") json = true;
        else if (a == "--int") fmt = FMT_INT;
        else if (a == "--uint") fmt = FMT_UINT;
        else if (a == "--fnv") algo = ALGO_FNV;
        else if (a == "--fold") fold = true;
        else if (a == "--seed") seed = static_cast<uint32_t>(std::strtoul(next("--seed").c_str(), nullptr, 0));
        else if (a == "--bone") bone = next("--bone");
        else if (a == "--reverse" || a == "--rev") { rev_str = next("--reverse"); have_rev = true; }
        else if (a == "--prefix") prefix = next("--prefix");
        else if (a == "--max") maxlen = std::atoi(next("--max").c_str());
        else if (a == "--charset") charset_str = next("--charset");
        else if (a == "-h" || a == "--help" || a == "help") { print_usage(); return 0; }
        else if (a == "--") { if (++i < argc && text.empty()) text = argv[i]; }
        else if (text.empty()) text = a;
        else { std::cerr << "error: 多余参数: " << a << std::endl; return 2; }
    }

    // ---------- reverse 模式 ----------
    if (have_rev) {
        if (fmt != FMT_HEX)
            std::cerr << "note: --int/--uint 在 --reverse 模式下不生效（已忽略）" << std::endl;
        if (algo != ALGO_FNV && seed != FNV_DEFAULT_SEED)
            std::cerr << "note: --seed 仅在 --fnv 下生效（已忽略）" << std::endl;
        if (algo != ALGO_FNV && fold)
            std::cerr << "note: --fold 仅在 --fnv 下生效（已忽略）" << std::endl;
        if (maxlen < 1) maxlen = 1;
        if (maxlen > 6) { std::cerr << "warning: --max 上限为 6" << std::endl; maxlen = 6; }
        std::string charset = charset_str.empty() ? "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_" : charset_str;
        {   // 去重
            std::string d;
            for (char c : charset) if (d.find(c) == std::string::npos) d += c;
            charset = d;
        }
        if (charset.empty()) { std::cerr << "error: charset 为空" << std::endl; return 2; }

        // 目标解析：支持逗号分隔多目标批量反推
        std::vector<uint32_t> targets;
        {
            size_t pos = 0;
            while (pos <= rev_str.size()) {
                size_t comma = rev_str.find(',', pos);
                std::string tok = (comma == std::string::npos) ? rev_str.substr(pos) : rev_str.substr(pos, comma - pos);
                if (!tok.empty()) {
                    char* end = nullptr;
                    unsigned long t = std::strtoul(tok.c_str(), &end, 0);   // 0x.. 或十进制
                    if (end == tok.c_str() || *end != '\0') { std::cerr << "error: 无法解析目标值: " << tok << std::endl; return 2; }
                    if (targets.size() >= 32) { std::cerr << "error: 目标数上限 32" << std::endl; return 2; }
                    targets.push_back(static_cast<uint32_t>(t));
                }
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            if (targets.empty()) { std::cerr << "error: 缺少目标值" << std::endl; return 2; }
        }

        uint32_t prefix_state = (algo == ALGO_FNV)
            ? fnv1a32(prefix.c_str(), prefix.size(), seed, fold)
            : (prefix.empty() ? 0xFFFFFFFFu : ComputeHash(prefix.c_str()));

        auto verify_full = [&](const std::string& full) -> uint32_t {
            return algo == ALGO_FNV ? fnv1a32(full.c_str(), full.size(), seed, fold)
                                    : ComputeHash(full.c_str());
        };

        bool truncated = false;
        std::vector<std::vector<std::string>> hits;
        reverse_search(prefix_state, targets, charset, maxlen, algo, truncated, hits);

        double combos = 0.0;
        for (int L = 0; L <= maxlen; ++L) { double c = 1.0; for (int i = 0; i < L; ++i) c *= static_cast<double>(charset.size()); combos += c; }

        if (json) {
            std::cout << "{\"ok\":true,\"mode\":\"reverse\",\"algo\":\"" << (algo == ALGO_FNV ? "fnv" : "crc")
                      << "\",\"prefix\":\"" << json_escape(prefix)
                      << "\",\"charsetLen\":" << charset.size()
                      << ",\"maxLen\":" << maxlen
                      << ",\"space\":" << static_cast<unsigned long long>(combos)
                      << ",\"truncated\":" << (truncated ? "true" : "false");
            if (targets.size() == 1) {
                std::cout << ",\"target\":\"" << hex32(targets[0])
                          << "\",\"count\":" << hits[0].size()
                          << ",\"found\":[";
                for (size_t i = 0; i < hits[0].size(); ++i) {
                    if (i) std::cout << ",";
                    std::cout << "\"" << json_escape(hits[0][i]) << "\"";
                }
                std::cout << "]";
            } else {
                std::cout << ",\"results\":[";
                for (size_t t = 0; t < targets.size(); ++t) {
                    if (t) std::cout << ",";
                    std::cout << "{\"target\":\"" << hex32(targets[t])
                              << "\",\"count\":" << hits[t].size()
                              << ",\"found\":[";
                    for (size_t i = 0; i < hits[t].size(); ++i) {
                        if (i) std::cout << ",";
                        std::cout << "\"" << json_escape(hits[t][i]) << "\"";
                    }
                    std::cout << "]}";
                }
                std::cout << "]";
            }
            std::cout << "}" << std::endl;
        }
        else {
            std::cout << "algo     : " << (algo == ALGO_FNV ? "FNV-1a" : "CRC32 (raw 语义)") << std::endl;
            std::cout << "prefix   : \"" << prefix << "\"  prefix_state=" << hex32(prefix_state) << std::endl;
            std::cout << "charset  : " << charset.size() << " 字符, maxlen=" << maxlen
                      << ", 搜索空间 ~" << static_cast<unsigned long long>(combos)
                      << (truncated ? "  (截断)" : "") << std::endl;
            for (size_t t = 0; t < targets.size(); ++t) {
                std::cout << "target[" << t << "] : " << hex32(targets[t]) << "  found " << hits[t].size() << std::endl;
                for (const auto& h : hits[t]) {
                    std::string full = prefix + h;
                    std::cout << "  " << (h.empty() ? "(空后缀)" : h) << "   ->  " << full
                              << "  verify=" << hex32(verify_full(full)) << std::endl;
                }
            }
        }
        return 0;
    }

    // ---------- bone 模式 ----------
    if (!bone.empty()) {
        if (text.empty()) { std::cerr << "error: bone 模式需要轨道名作为输入文本" << std::endl; return 2; }
        std::vector<std::string> path;
        size_t start = 0;
        while (start <= bone.size()) {
            size_t pos = bone.find('/', start);
            std::string part = (pos == std::string::npos) ? bone.substr(start) : bone.substr(start, pos - start);
            if (part.empty()) { std::cerr << "error: --bone 路径含空段: " << bone << std::endl; return 2; }
            path.push_back(part);
            if (pos == std::string::npos) break;
            start = pos + 1;
        }
        const bool is_fnv = (algo == ALGO_FNV);
        uint32_t h = 0;
        bool ok = is_fnv ? bone_hierarchy_hash_fnv(path, text, seed, fold, h)
                         : bone_hierarchy_hash(path, text, h);
        if (!ok) { std::cerr << "error: bone 路径为空" << std::endl; return 2; }

        if (json) {
            std::cout << "{\"ok\":true,\"mode\":\"bone\",\"algo\":\"" << (is_fnv ? "fnv" : "crc")
                      << "\",\"track\":\"" << json_escape(text)
                      << "\",\"bone\":\"" << json_escape(bone)
                      << "\",\"order\":[";
            for (size_t i = path.size(); i-- > 0; ) {
                std::cout << "\"" << json_escape(path[i]) << "\"";
                if (i) std::cout << ",";
            }
            std::cout << "],\"fmt\":\"" << fmt_name(fmt)
                      << "\",\"hash\":\"" << hex32(h)
                      << "\",\"hashValue\":" << json_num32(h, fmt);
            if (!is_fnv)
                std::cout << ",\"raw\":\"" << hex32(h ^ 0xFFFFFFFFu)
                          << "\",\"rawValue\":" << json_num32(h ^ 0xFFFFFFFFu, fmt);
            std::cout << "}" << std::endl;
        }
        else {
            std::cout << "algo     : " << (is_fnv ? "FNV-1a" : "CRC32 (原 calculate_bone_hierarchy_hash)") << std::endl;
            std::cout << "bone     : " << bone << "  (叶->根: ";
            for (size_t i = path.size(); i-- > 0; ) { std::cout << path[i]; if (i) std::cout << " -> "; }
            std::cout << ")" << std::endl;
            std::cout << "track    : " << text << std::endl;
            std::cout << "hash     : " << fmt_u32(h, fmt) << std::endl;
            if (!is_fnv)
                std::cout << "raw      : " << fmt_u32(h ^ 0xFFFFFFFFu, fmt) << std::endl;
        }
        return 0;
    }

    // ---------- 普通 hash 模式 ----------
    if (text.empty()) { print_usage(); return 2; }
    const bool is_fnv = (algo == ALGO_FNV);
    uint32_t raw = is_fnv ? fnv1a32(text.c_str(), text.size(), seed, fold)
                          : ComputeHash(text.c_str());

    if (json) {
        std::cout << "{\"ok\":true,\"mode\":\"hash\",\"algo\":\"" << (is_fnv ? "fnv" : "crc")
                  << "\",\"input\":\"" << json_escape(text)
                  << "\",\"fmt\":\"" << fmt_name(fmt);
        if (is_fnv) {
            std::cout << "\",\"seed\":\"" << hex32(seed)
                      << "\",\"fold\":" << (fold ? "true" : "false")
                      << ",\"raw\":\"" << hex32(raw)
                      << "\",\"rawValue\":" << json_num32(raw, fmt);
        } else {
            uint32_t stdv = raw ^ 0xFFFFFFFFu;
            std::cout << "\",\"raw\":\"" << hex32(raw)
                      << "\",\"std\":\"" << hex32(stdv)
                      << "\",\"rawValue\":" << json_num32(raw, fmt)
                      << ",\"stdValue\":" << json_num32(stdv, fmt);
        }
        std::cout << "}" << std::endl;
    }
    else {
        std::cout << "input    : " << text << std::endl;
        if (is_fnv) {
            std::cout << "algo     : FNV-1a (seed " << hex32(seed) << ", fold=" << (fold ? "on" : "off") << ")" << std::endl;
            std::cout << "fnv      : " << fmt_u32(raw, fmt) << std::endl;
        } else {
            uint32_t stdv = raw ^ 0xFFFFFFFFu;
            std::cout << "raw      : " << fmt_u32(raw, fmt) << "   (ComputeHash, 无最终异或, Civ 风格)" << std::endl;
            std::cout << "crc32std : " << fmt_u32(stdv, fmt) << "   (标准 CRC32 = raw ^ 0xFFFFFFFF)" << std::endl;
        }
    }
    return 0;
}
