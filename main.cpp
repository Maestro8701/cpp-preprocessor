#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

// напишите эту функцию
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories);

bool SearchIncludeDirectories(const path& dest_file, const vector<path>& inc_dirs, const path& target_include) {
    bool include_found = false;
    for (const auto& inc_dir : inc_dirs) {
        path full_path = inc_dir / target_include;
        if (!exists(full_path)) {
            continue;
        }
        include_found = true;
        Preprocess(full_path, dest_file, inc_dirs);
        break;
    }
    return include_found;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    static regex local_include (R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    smatch match_local_include; 
    static regex system_include (R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch match_system_include;

    ifstream input(in_file, ios::in);
    if (!input.is_open()) {
    cerr << "Ошибка: не удалось открыть входной файл " << in_file << endl;
    return false;
}
    ofstream output(out_file, ios::out | ios::app);
    if (!output.is_open()) {
    cerr << "Ошибка: не удалось открыть выходной файл " << out_file << endl;
    return false;
}
    int line_num=0;
    string line;

    while (getline(input, line)) {
        ++ line_num;
        ifstream input_file;

        if (regex_match(line, match_local_include, local_include)) {
            path include_file_name = string(match_local_include[1]);
            path file_parent_directory = in_file.parent_path();
            path resolved_include_path = file_parent_directory / include_file_name;
            input_file.open(resolved_include_path);

            if (input_file.is_open()) {
                Preprocess(resolved_include_path, out_file, include_directories);
                input_file.close();
            } else {
                bool found = SearchIncludeDirectories(out_file, include_directories, include_file_name);
                if (!found) {
                    cout << "unknown include file "s << include_file_name.string() << " at file "s << in_file.string() << " at line "s << line_num << endl;
                    return false;
                }
            }
        } else if (regex_match(line, match_system_include, system_include)) {
            path include_file_name = string(match_system_include[1]);
            bool found = SearchIncludeDirectories(out_file, include_directories, include_file_name);
            if (!found) {
                cout << "unknown include file "s << include_file_name.string() << " at file "s << in_file.string() << " at line "s << line_num << endl;
                return false;
            }
        } else {
            output << line << endl;
        }
    }
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
