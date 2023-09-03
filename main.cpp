#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>

using namespace std;
using filesystem::path;

static const regex local_l_r(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
static const regex global_l_r(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

// напишите эту функцию
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories);

bool PreprocessDirs(ostream& out_stream, const vector<path>& include_directories, const std::string& cur_include);

bool Preprocess(istream& in_stream, ostream& out_stream, const path& cur_path, const vector<path>& include_directories);

void PrintError(const std::string& cur_include, const std::string& in_file, int line);

string GetFileContents(const string& file) {
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

bool PreprocessDirs(ostream& out_stream, const vector<path>& include_directories, const std::string& cur_include){
    ifstream in_stream;
    for (const auto &in_dir: include_directories) {
        path in_path = in_dir / cur_include;
        in_stream.open(in_path);
        if(!in_stream.is_open()){
            continue;
        }
        if(Preprocess(in_stream, out_stream, in_path, include_directories)){
            return true;
        }
    }
    return false;
}

void PrintError(const std::string& cur_include, const std::string& in_file, int line){
    cout << "unknown include file "<< cur_include <<" at file " << in_file << " at line " << line << endl;
}

bool Preprocess(istream& in_stream, ostream& out_stream, const path& cur_path, const vector<path>& include_directories){
    string cur_line;

    int line = 1;
    while(getline(in_stream, cur_line)) {
        smatch cur_match;
        if (regex_match(cur_line, cur_match, local_l_r)) {
            auto next_include = cur_match[1].str();
            path next_path = cur_path.parent_path() / next_include;
            ifstream next_in_stream(next_path);
            if(!next_in_stream.is_open() || !Preprocess(next_in_stream, out_stream, next_path, include_directories)){
                if(!PreprocessDirs(out_stream, include_directories, next_include)){
                    PrintError(next_include, cur_path.string(), line);
                    return false;
                }
            }
        }
        else if (regex_match(cur_line, cur_match, global_l_r)) {
            auto cur_include = cur_match[1].str();
            if(!PreprocessDirs(out_stream, include_directories, cur_include)){
                PrintError(cur_include, cur_path.string(), line);
                return false;
            }
        }
        else {
            out_stream << cur_line << "\n";
        }
        ++line;
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories){
    ofstream out_stream(out_file);
    if(!out_stream.is_open()){
        return false;
    }

    ifstream in_stream(in_file);
    if(!in_stream.is_open()){
        return false;
    }

    return Preprocess(in_stream, out_stream, in_file, include_directories);
}