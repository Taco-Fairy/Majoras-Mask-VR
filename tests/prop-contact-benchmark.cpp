#include "solid_hull.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char**argv) {
 if(argc!=2)return 2;
 std::ifstream input(argv[1]);std::vector<mmvr::HandPoint> points;mmvr::HandPoint p;
 while(input>>p[0]>>p[1]>>p[2])points.push_back(p);
 if(points.size()<4)throw std::runtime_error("Missing mesh fixture");
 mmvr::SolidHull hull;hull.Build(points);
 std::vector<mmvr::HandPoint> queries;
 for(int i=0;i<30000;++i)queries.push_back({float((i*97)%1800-900),float((i*37)%1800-900),float((i*53)%1800-900)});
 std::vector<bool> reference;int hits=0;double timing[2]{};
 for(int mode=0;mode<2;++mode) {
  auto start=std::chrono::steady_clock::now();
  for(size_t i=0;i<queries.size();++i) {
   mmvr::HandPoint q;bool inside=false;
   if(!hull.Closest(queries[i],q,inside,mode?15.f:-1.f))throw std::runtime_error("No hull");
   bool hit=inside||mmvr::HandLength(mmvr::HandSub(queries[i],q))<15.f;
   if(!mode)reference.push_back(hit);else if(hit!=reference[i])throw std::runtime_error("Contact changed");
   if(mode&&hit)++hits;
  }
  timing[mode]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 }
 std::cout<<"{\"queries\":"<<queries.size()<<",\"hits\":"<<hits<<",\"baselineMs\":"<<timing[0]<<",\"boundedMs\":"<<timing[1]<<",\"sameContacts\":true}\n";
}
