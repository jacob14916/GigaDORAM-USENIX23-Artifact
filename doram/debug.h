#ifndef DORAM_DEBUG_H__
#define DORAM_DEBUG_H__

#include "globals.h"
#include <iostream>

using namespace std;
namespace emp
{

template<typename S, typename T>
ostream& operator << (ostream& stream, const pair<S, T> &v) {
    return stream << "(" << v.first << ", " << v.second << ")";
}

template<typename T>
ostream& operator << (ostream& stream, const vector<T> &v) {
    stream << "[";
    string sep = "";
    for (const T& t: v) {stream << sep << t; sep = ", ";}
    return stream << "]";
}

void dbg_out()
{
    cerr << "\n"; // removed endl to allow more efficient printing
}
template <typename Head, typename... Tail> void dbg_out(Head H, Tail... T)
{
    cerr << ' ' << H;
    dbg_out(T...);
}

#if defined(DORAM_DEBUG) || defined(DORAM_DEBUG_CMAKE)
#define dbg(desc, ...)                                                                                                 \
    { cerr << '(' << party << " " << desc << "):";                                                                       \
    dbg_out(__VA_ARGS__); }
#define dbg_args(...) dbg(#__VA_ARGS__, __VA_ARGS__)
#else
#define dbg(...)
#define dbg_args(...)
#endif // DORAM_DEBUG

} // namespace emp
#endif
