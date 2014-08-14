#pragma once

#include <Eigen/Dense>

//#include <boost/random/uniform_int_distribution.hpp>
//#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_01.hpp>

#include "distribution.hpp"
#include "sampler.hpp"

using namespace Eigen;
using namespace std;

template<typename T>
class Mult : public Distribution<T>
{
public:
  uint32_t K_;
  Matrix<T,Dynamic,1> pdf_;

  /* constructor from pdf */
  Mult(const Matrix<T,Dynamic,1>& pdf, boost::mt19937 *pRndGen);
  /* constructor from indicators - estimates from counts */
  Mult(const VectorXu& z, boost::mt19937 *pRndGen);
  /* copy constructor */
  Mult(const Mult& other);
  virtual ~Mult();

  uint32_t sample();
  void sample(VectorXu& z);

  T logPdf(const Matrix<T,Dynamic,1>& x) const;

  const Matrix<T,Dynamic,1>& pdf() const {return pdf_;};
  void pdf(const Matrix<T,Dynamic,1>& pdf){
    pdf_ = pdf;
  };

  void print() const;

private:
  boost::uniform_01<T> unif_;
};

typedef Mult<float> Multf;
typedef Mult<double> Multd;
