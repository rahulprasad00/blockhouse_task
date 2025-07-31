// reconstruct.cpp
// Compile with: g++ -std=c++17 -O2 reconstruct.cpp -o reconstruct

#include <bits/stdc++.h>
using namespace std;

struct Order { char side; double price; int qty; };
typedef pair<int,int> QtyCount;

// Simple CSV line splitter (no handling of quoted commas)
vector<string> splitCSV(const string &line) {
    vector<string> res;
    string cur;
    for (char c : line) {
        if (c == ',') {
            res.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    res.push_back(cur);
    return res;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input_mbo.csv> <output_mbp.csv>\n";
        return 1;
    }
    ifstream fin(argv[1]);
    ofstream fout(argv[2]);
    if (!fin.is_open() || !fout.is_open()) {
        cerr << "Failed to open input or output file\n";
        return 1;
    }
    string line;
    // Read and discard input header
    getline(fin, line);

    // Write MBP-10 header matching expected mbp.csv
    fout << "ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence";
    for (int i = 0; i < 10; ++i) {
        fout << ",bid_px_" << setw(2) << setfill('0') << i
             << ",bid_sz_" << setw(2) << setfill('0') << i
             << ",bid_ct_" << setw(2) << setfill('0') << i;
    }
    for (int i = 0; i < 10; ++i) {
        fout << ",ask_px_" << setw(2) << setfill('0') << i
             << ",ask_sz_" << setw(2) << setfill('0') << i
             << ",ask_ct_" << setw(2) << setfill('0') << i;
    }
    fout << ",symbol,order_id\n";

    unordered_map<int, Order> orders;
    map<double, QtyCount, greater<double>> bids;
    map<double, QtyCount> asks;

    int pendingTradeId = -1;
    bool pendingFillDone = false;

    while (getline(fin, line)) {
        auto cols = splitCSV(line);
        if (cols.size() < 15) continue;
        // Only process MBO records
        string rtype = cols[2];
        if (rtype != "160") continue;

        string ts_recv    = cols[0];
        string ts_event   = cols[1];
        string publisher  = cols[3];
        string instrument = cols[4];
        char action       = cols[5].empty() ? ' ' : cols[5][0];
        char side         = cols[6].empty() ? ' ' : cols[6][0];
        string priceStr   = cols[7];
        double price      = priceStr.empty() ? 0.0 : stod(priceStr);
        string sizeStr    = cols[8];
        int size          = sizeStr.empty() ? 0 : stoi(sizeStr);
        int order_id      = stoi(cols[9]);
        string flags      = cols[10];
        string ts_in_delta= cols[12];
        string sequence   = cols[13];
        string symbol     = cols[14];

        // Handle T-F-C properly
        if (action == 'T') {
            pendingTradeId = order_id;
            pendingFillDone = false;
            continue;
        }
        if (action == 'F' && order_id == pendingTradeId && !pendingFillDone) {
            // Apply fill to resting order only once
            auto itOrder = orders.find(order_id);
            if (itOrder != orders.end()) {
                Order &ord = itOrder->second;
                ord.qty -= size;
                if (ord.side == 'B') bids[ord.price].first -= size;
                else if (ord.side == 'S') asks[ord.price].first -= size;
                if (ord.qty <= 0) {
                    if (ord.side == 'B') {
                        auto &p = bids[ord.price]; p.second -= 1;
                        if (p.second == 0) bids.erase(ord.price);
                    } else {
                        auto &p = asks[ord.price]; p.second -= 1;
                        if (p.second == 0) asks.erase(ord.price);
                    }
                    orders.erase(itOrder);
                }
            }
            pendingFillDone = true;
            continue;
        }
        if (action == 'C' && order_id == pendingTradeId && pendingFillDone) {
            // Skip cancel of remaining aggressor
            pendingTradeId = -1;
            continue;
        }

        // Regular Adds/Mods/Cancels
        auto itOrder = orders.find(order_id);
        if (action == 'A') {
            orders[order_id] = {side, price, size};
            if (side == 'B') bids[price].first += size, bids[price].second += 1;
            else if (side == 'S') asks[price].first += size, asks[price].second += 1;
        } else if (action == 'M' && itOrder != orders.end()) {
            Order old = itOrder->second;
            if (old.side == 'B') {
                auto &p = bids[old.price]; p.first -= old.qty; p.second -= 1;
                if (p.second == 0) bids.erase(old.price);
            } else if (old.side == 'S') {
                auto &p = asks[old.price]; p.first -= old.qty; p.second -= 1;
                if (p.second == 0) asks.erase(old.price);
            }
            orders[order_id] = {side, price, size};
            if (side == 'B') bids[price].first += size, bids[price].second += 1;
            else if (side == 'S') asks[price].first += size, asks[price].second += 1;
        } else if (action == 'C' && itOrder != orders.end()) {
            Order &ord = itOrder->second;
            ord.qty -= size;
            if (ord.side == 'B') bids[ord.price].first -= size;
            else if (ord.side == 'S') asks[ord.price].first -= size;
            if (ord.qty <= 0) {
                if (ord.side == 'B') {
                    auto &p = bids[ord.price]; p.second -= 1;
                    if (p.second == 0) bids.erase(ord.price);
                } else {
                    auto &p = asks[ord.price]; p.second -= 1;
                    if (p.second == 0) asks.erase(ord.price);
                }
                orders.erase(itOrder);
            }
        }

        // Snapshot top-10
        vector<pair<double,QtyCount>> topBids, topAsks;
        for (auto it = bids.begin(); it != bids.end() && topBids.size() < 10; ++it)
            topBids.push_back(*it);
        for (auto it = asks.begin(); it != asks.end() && topAsks.size() < 10; ++it)
            topAsks.push_back(*it);

        // Emit MBP-10 row
        fout << ts_recv << ','
             << ts_event << ','
             << "10"     << ','  // rtype for MBP records
             << publisher << ','
             << instrument<< ','
             << action    << ','
             << side      << ','
             << "10"     << ','  // depth = 10 levels
             << priceStr  << ','
             << sizeStr   << ','
             << flags     << ','
             << ts_in_delta<< ','
             << sequence;
        for (int i = 0; i < 10; ++i) {
            if (i < (int)topBids.size()) {
                fout << ',' << fixed << setprecision(2) << topBids[i].first
                     << ',' << topBids[i].second.first
                     << ',' << topBids[i].second.second;
            } else {
                fout << ",,,";
            }
        }
        for (int i = 0; i < 10; ++i) {
            if (i < (int)topAsks.size()) {
                fout << ',' << fixed << setprecision(2) << topAsks[i].first
                     << ',' << topAsks[i].second.first
                     << ',' << topAsks[i].second.second;
            } else {
                fout << ",,,";
            }
        }
        fout << ',' << symbol << ',' << order_id << '\n';
    }
    fin.close();
    fout.close();
    return 0;
}
