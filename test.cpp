#include <string> 
#include <iostream> 
#include <regex>

using namespace std;

int main()
{
    string line;
    string output;
    while (getline(cin, line))
    {
        output += line + "\n";
    }

    regex re("(@@).+(@@)");
    sregex_iterator regex_delim = sregex_iterator(output.begin(), output.end(), re);
    sregex_iterator end = sregex_iterator();

    vector<string> chunks;

    for (sregex_iterator it = regex_delim; it != end; it++) {
        smatch match = *it;
        chunks.push_back(match.suffix());
    }

    // for (string chunk : chunks) {
    //     cout << chunk << endl;
    // }
    cout << chunks.size() << endl;

}