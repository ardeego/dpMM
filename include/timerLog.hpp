#pragma once

#include <string>
#include <iostream>
#include <vector>

#include <Eigen/Dense>
#include <timer.hpp>

using std::string;
using std::ofstream;
using std::vector;

class TimerLog : public Timer
{
public:
  TimerLog(string path, uint32_t N, string name="Timer0") 
    :  Timer(), path_(path), name_(name), fout_(path_.data(),ofstream::out), 
    t0s_(N), dts_(N,0),tSums_(N,0), tSquareSums_(N,0), Ns_(N,0) 
  {
    tic();
  };
  virtual ~TimerLog() {fout_.close();};

  virtual void tic(int32_t id=-1)
  {
    timeval t0 = this->getTimeOfDay();
    if(id < 0){
      for(int32_t i=0; i<static_cast<int32_t>(t0s_.size()); ++i) t0s_[i] = t0;
    } else if( 0<= id && id < static_cast<int32_t>(t0s_.size()))
      t0s_[id] = t0;
  };

  virtual void toc(int32_t id=-1)
  {
    timeval tE = this->getTimeOfDay();
    if(id < 0)
    {
      for(int32_t i=0; i<static_cast<int32_t>(dts_.size()); ++i) 
      {
        dts_[i] = this->getDtMs(t0s_[i],tE);
        tSums_[i] += dts_[i];
        tSquareSums_[i] += dts_[i]*dts_[i];
        Ns_[i] ++;
      }
    } else if( 0<= id && id < static_cast<int32_t>(t0s_.size()))
    {
        dts_[id] = this->getDtMs(t0s_[id],tE);
        tSums_[id] += dts_[id];
        tSquareSums_[id] += dts_[id]*dts_[id];
        Ns_[id] ++;
    }
  };

  virtual void toctic(int32_t id0, int32_t id1)
  {
    toc(id0);tic(id1);
  };

  virtual void logCycle()
  {
    for(uint32_t i=0; i<dts_.size()-1; ++i) 
    {
      fout_<<dts_[i]<<" ";
    }
    fout_<<dts_[dts_.size()-1]<<endl;
    fout_.flush();
  };

  virtual void printStats()
  {
    cout<<name_<<": stats over timer cycles (mean +- 3*std):\t";
    for(int32_t i=0; i<static_cast<int32_t>(dts_.size()); ++i) 
    {
      double mean = tSums_[i]/Ns_[i];
      double var = tSquareSums_[i]/Ns_[i] - mean*mean;
      cout<<mean<<"+- "<<3.*sqrt(var)<<"\t";
    } cout<<endl; 
  };

private:
  string path_;
  string name_;
  ofstream fout_;
  vector<timeval> t0s_; // starts for all timings
  vector<double> dts_; // dts
  vector<double> tSums_; // sum over the time
  vector<double> tSquareSums_; // sum over squares over the time
  vector<double> Ns_; // counts of observations for each dt
};

