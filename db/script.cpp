#include <unordered_set>
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <locale>

using std::unordered_set; using std::ifstream;
using std::cout; using std::ostream_iterator;
using std::tolower; using std::endl;
using std::remove_copy_if; using std::isalnum;

int main() {
	unordered_set<char> unique_chars;
	ifstream f("scat_codes.csv");
	char c;
	while (f.get(c)) {
		unique_chars.insert(tolower(c));
	}
	ostream_iterator<char> out(cout, "\n");
	remove_copy_if(unique_chars.begin(), unique_chars.end(), out, [](char c) { 
		return isalnum(c) || c == ' '; 
	});
	cout << endl;
}
