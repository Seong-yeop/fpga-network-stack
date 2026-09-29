// Unit check of the TX read-data stitcher: every first-access tail length (1..127 bytes) merged with second
// accesses of many lengths reproduces the byte stream, plus aligned merges and packets without a second access.
#include "../common/csim.hpp"
template <int WIDTH>
void read_data_stitching(hls::stream<bool>& memAccessBreakdown2readPkgStitcher, hls::stream<net_axis<WIDTH> >& readDataIn, hls::stream<net_axis<WIDTH> >& readDataOut);
static const int W = DATA_WIDTH;
static void feed(hls::stream<net_axis<W> > &s, const Bytes &b) {
 for (unsigned o = 0; o < b.size(); o += W/8) {
  net_axis<W> a; a.data = 0; a.keep = 0;
  for (unsigned j = 0; j < W/8 && o + j < b.size(); j++) { a.data(j*8+7, j*8) = b[o+j]; a.keep[j] = 1; }
  a.last = o + W/8 >= b.size(); s.write(a);
 }
}
static void stitch_case(unsigned n1, unsigned n2, unsigned seed) {
 hls::stream<bool> merge; hls::stream<net_axis<W> > in, out;
 Bytes a(n1), b(n2);
 for (unsigned i = 0; i < n1; i++) a[i] = (i * 7 + seed) & 0xff;
 for (unsigned i = 0; i < n2; i++) b[i] = (i * 13 + seed + 101) & 0xff;
 merge.write(n2 > 0); feed(in, a); if (n2) feed(in, b);
 Bytes expect(a); expect.insert(expect.end(), b.begin(), b.end());
 Bytes got; bool last = false; unsigned words = 0;
 for (unsigned t = 0; t < 4096 && !last; t++) {
  read_data_stitching<W>(merge, in, out);
  while (!out.empty()) {
   net_axis<W> w = out.read(); words++;
   bool gap = false, seen = false;
   for (unsigned j = 0; j < W/8; j++) { if (w.keep[j]) { if (gap) check(false, "keep gap"); got.push_back(w.data(j*8+7, j*8)); seen = true; } else if (seen) gap = true; }
   check(seen, "empty word");
   if (w.last) last = true;
  }
 }
 std::string ctx = " n1=" + std::to_string(n1) + " n2=" + std::to_string(n2);
 check(last, ("missing last" + ctx).c_str());
 check(got == expect, ("stitched bytes differ" + ctx).c_str());
 check(in.empty() && merge.empty() && out.empty(), ("leftover input" + ctx).c_str());
 check(words == (expect.size() + W/8 - 1) / (W/8), ("word count" + ctx).c_str());
}
static void test() {
 unsigned seed = 1;
 for (unsigned tail = 1; tail < W/8; tail++)
  for (unsigned n2 : {1u, 3u, unsigned(W/8) - tail, unsigned(W/8) - tail + 1, unsigned(W/8), 200u, 2u * W/8 + tail, 300u})
   stitch_case(W/8 + tail, n2, seed++);
 for (unsigned n2 : {1u, unsigned(W/8), 257u}) stitch_case(2 * W/8, n2, seed++);
 for (unsigned n1 : {1u, 5u, unsigned(W/8), unsigned(W/8) + 1, 3u * W/8 - 2}) stitch_case(n1, 0, seed++);
 for (unsigned tail = 1; tail < W/8; tail += 17) stitch_case(tail, 5 * W/8 + 9, seed++);
}
int main(){return run_case("stitch",test);}
