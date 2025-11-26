#include <fstream>
#include <string>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <ctime>

using std::ifstream; using std::ofstream;
using std::getline; using std::find; using std::string;
using std::stoi; using std::copy; using std::cout; using std::endl;
using std::ostream_iterator; using std::copy_n;
using std::unordered_map;

void convert_to_sqlite_date(string &date) {
	char converted[11];
	converted[10] = '\0';
	struct tm tm;
	strptime(date.c_str(), "%d-%b-%Y", &tm);
	strftime(converted, 10, "%Y-%m-%d", &tm);
	date = string(converted);
}

int main(int argc, char *argv[]) {
	ifstream fin(argv[1]);
	ofstream fout_related("related.csv"), fout_items("items.csv"), 
			 fout_additional_items("additional_items.csv"), fout_plant_machine("plant_machine.csv"), 
			 fout_parking("parking.csv"), fout_adj("adj.csv"), fout_adj_tot("adj_tot.csv");
	ofstream *fout_p;
	string line, field1, field2, date_from, fk;
	string::iterator lim1, lim2, date_from_beg, date_from_end;
	int i = 0;
	while (getline(fin, line)) {
		cout << "line " << i++ << endl;
		lim1 = find(line.begin(), line.end(), '*');
		field1 = string(line.begin(), lim1);
		if (field1 == "01") {
			lim2 = find(lim1+1, line.end(), '*');
			field2 = string(lim1+1, lim2);
			fk = std::move(field2);

			// convert from YYYY-MMM-DD 
			date_from_beg = lim2;
			for (int i = 0; i != 22; i++) {
				date_from_beg = find(date_from_beg+1, line.end(), '*');
			}
			date_from_beg++;
			date_from_end = find(date_from_beg, line.end(), '*');
			date_from = string(date_from_beg, date_from_end);
			convert_to_sqlite_date(date_from);
			copy(date_from.begin(), date_from.end(), date_from_beg);
			line.erase(date_from_end-1);
		} else {
			for (int i = 0; i != fk.size()+1; i++) {
				line.push_back('-');
			}
			copy(line.rbegin()+fk.size()+1, line.rend(), line.rbegin());
			copy(fk.begin(), fk.end(), line.begin());
			line[fk.size()] = '*';
		}

		switch(stoi(field1)) {
			case 1:
				fout_p = &fout_related;
				break;
			case 2:
				fout_p = &fout_items;
				break;
			case 3:
				fout_p = &fout_additional_items;
				break;
			case 4:
				fout_p = &fout_plant_machine;
				break;
			case 5:
				fout_p = &fout_parking;
				break;
			case 6:
				fout_p = &fout_adj;
				break;
			case 7:
				fout_p = &fout_adj_tot;
				break;
			default:
				continue;
		}
		*fout_p << line << '\n';
	}
	return 0;
}
