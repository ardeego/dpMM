#pragma once
#include <iostream>
#include <stdint.h>
#include <vector>
#include <Eigen/Dense>

#include <boost/shared_ptr.hpp>

#include "dpMM.hpp"
#include "cat.hpp"
#include "dir.hpp"
#include "niw.hpp"
#include "sampler.hpp"
#include "basemeasure.hpp"

using namespace Eigen;
using std::cout;
using std::endl; 
using boost::shared_ptr;
using std::vector; 

template<typename T=double>
class DirNaiveBayes : public DpMM<T>{

public:
  DirNaiveBayes(const Dir<Cat<T>, T>& alpha, const boost::shared_ptr<BaseMeasure<T> >& theta);
  DirNaiveBayes(const Dir<Cat<T>, T>& alpha, const vector<boost::shared_ptr<BaseMeasure<T> > >& thetas);
  virtual ~DirNaiveBayes();

  virtual void initialize(const vector< Matrix<T,Dynamic,Dynamic> >& x);
  virtual void initialize(const boost::shared_ptr<ClData<T> >& cld)
    {cout<<"not supported"<<endl; assert(false);};

  virtual void sampleLabels();
  virtual void sampleParameters();

  virtual T logJoint(bool verbose=false);
  virtual const VectorXu& labels(){return z_;};
  virtual const VectorXu& getLabels(){return z_;};
  virtual uint32_t getK() const { return K_;};

//  virtual MatrixXu mostLikelyInds(uint32_t n);
  virtual MatrixXu mostLikelyInds(uint32_t n, Matrix<T,Dynamic,Dynamic>& logLikes);

  Matrix<T,Dynamic,1> getCounts();

  virtual void inferAll(uint32_t nIter, bool verbose=false);

protected: 
  uint32_t K_;
  Dir<Cat<T>, T> dir_;
  Cat<T> pi_;
#ifdef CUDA
  SamplerGpu<T>* sampler_;
#else 
  Sampler<T>* sampler_;
#endif
  Matrix<T,Dynamic,Dynamic> pdfs_;
//  Cat cat_;
  vector<boost::shared_ptr<BaseMeasure<T> > > thetas_;

  vector<Matrix<T,Dynamic,Dynamic>> x_;
  VectorXu z_;
};

// --------------------------------------- impl -------------------------------


template<typename T>
DirNaiveBayes<T>::DirNaiveBayes(const Dir<Cat<T>,T>& alpha, const boost::shared_ptr<BaseMeasure<T> >& theta) :
  K_(alpha.K_), dir_(alpha), pi_(dir_.sample()) //cat_(dir_.sample()),
{
// init the parameters
    cout<<"[DirNaiveBayes::DirNaiveBayes] creating thetas (you only gave me one)"<<endl;
    for (uint32_t k=0; k<K_; ++k)
      thetas_.push_back(boost::shared_ptr<BaseMeasure<T> >(theta->copy()));
};


template<typename T>
DirNaiveBayes<T>::DirNaiveBayes(const Dir<Cat<T>,T>& alpha, 
    const vector<boost::shared_ptr<BaseMeasure<T> > >& thetas) :
  K_(alpha.K_), dir_(alpha), pi_(dir_.sample()), //cat_(dir_.sample()),
  thetas_(thetas)
{};


template<typename T>
DirNaiveBayes<T>::~DirNaiveBayes()
{
  if (sampler_ != NULL) delete sampler_;
};

template <typename T>
Matrix<T,Dynamic,1> DirNaiveBayes<T>::getCounts()
{
  return counts<T,uint32_t>(z_,K_);
};


template<typename T>
void DirNaiveBayes<T>::initialize(const vector< Matrix<T,Dynamic,Dynamic> > &x)
{
  cout<<"init"<<endl;
  x_ = x;
  // randomly init labels from prior
  z_.setZero(x.size());
  cout<<"sample pi"<<endl;
  pi_ = dir_.sample(); 
  cout<<"init pi="<<pi_.pdf().transpose()<<endl;
  pi_.sample(z_);

  pdfs_.setZero(x.size(),K_);
#ifdef CUDA
  sampler_ = new SamplerGpu<T>(x.size(),K_,dir_.pRndGen_);
#else 
  sampler_ = new Sampler<T>(dir_.pRndGen_);
#endif

#pragma omp parallel for
  for(uint32_t k=0; k<K_; ++k)
    thetas_[k]->posterior(x_,z_,k);
};

template<typename T>
void DirNaiveBayes<T>::sampleLabels()
{
  // obtain posterior categorical under labels
  pi_ = dir_.posterior(z_).sample();
//  cout<<pi_.pdf().transpose()<<endl;
  
#pragma omp parallel for
  for(uint32_t i=0; i<z_.size(); ++i)
  {
    //TODO: could buffer this better
    // compute categorical distribution over label z_i
    VectorXd logPdf_z = pi_.pdf().array().log();
    for(uint32_t k=0; k<K_; ++k)
    {
//      cout<<thetas_[k].logLikelihood(x_.col(i))<<" ";
      logPdf_z[k] += thetas_[k]->logLikelihood(x_[i]);
    }
//    cout<<endl;
    // make pdf sum to 1. and exponentiate
    pdfs_.row(i) = (logPdf_z.array()-logSumExp(logPdf_z)).exp().matrix().transpose();
//    cout<<pi_.pdf().transpose()<<endl;
//    cout<<pdf.transpose()<<" |.|="<<pdf.sum();
//    cout<<" z_i="<<z_[i]<<endl;
  }
  // sample z_i
  sampler_->sampleDiscPdf(pdfs_,z_);
};


template<typename T>
void DirNaiveBayes<T>::sampleParameters()
{
#pragma omp parallel for 
  for(uint32_t k=0; k<K_; ++k)
  {
    thetas_[k]->posterior(x_,z_,k);
//    cout<<"k:"<<k<<endl;
//    thetas_[k]->print();
  }
};


template<typename T>
T DirNaiveBayes<T>::logJoint(bool verbose)
{
  T logJoint = dir_.logPdf(pi_);
  if(verbose)
  	cout<<"log p(pi)="<<logJoint<<" -> ";

#pragma omp parallel for reduction(+:logJoint)  
  for (uint32_t k=0; k<K_; ++k)
    logJoint = logJoint + thetas_[k]->logPdfUnderPrior();
	if(verbose)
		cout<<"log p(pi)*p(theta)="<<logJoint<<" -> ";

#pragma omp parallel for reduction(+:logJoint)  
  for (uint32_t i=0; i<z_.size(); ++i)
    logJoint = logJoint + thetas_[z_[i]]->logLikelihood(x_[i]);
  if(verbose)
  	cout<<"log p(phi)*p(theta)*p(x|z,theta)="<<logJoint<<"]"<<endl;
  
  return logJoint;
};


template<typename T>
MatrixXu DirNaiveBayes<T>::mostLikelyInds(uint32_t n, Matrix<T,Dynamic,Dynamic>& logLikes)
{
  MatrixXu inds = MatrixXu::Zero(n,K_);
  logLikes = Matrix<T,Dynamic,Dynamic>::Ones(n,K_);
  logLikes *= -99999.0;
  
#pragma omp parallel for 
  for (uint32_t k=0; k<K_; ++k)
  {
    for (uint32_t i=0; i<z_.size(); ++i)
      if(z_(i) == k)
      {
        T logLike = thetas_[z_[i]]->logLikelihood(x_[i]);
        for (uint32_t j=0; j<n; ++j)
          if(logLikes(j,k) < logLike)
          {
            for(uint32_t l=n-1; l>j; --l)
            {
              logLikes(l,k) = logLikes(l-1,k);
              inds(l,k) = inds(l-1,k);
            }
            logLikes(j,k) = logLike;
            inds(j,k) = i;
//            cout<<"after update "<<logLike<<endl;
//            Matrix<T,Dynamic,Dynamic> out(n,K_*2);
//            out<<logLikes.cast<T>(),inds.cast<T>();
//            cout<<out<<endl;
            break;
          }
      }
  } 
  cout<<"::mostLikelyInds: logLikes"<<endl;
  cout<<logLikes<<endl;
  cout<<"::mostLikelyInds: inds"<<endl;
  cout<<inds<<endl;
  return inds;
};


template<typename T>
void DirNaiveBayes<T>::inferAll(uint32_t nIter, bool verbose)
{ 
  if(verbose){
  	cout<<"[DirNaiveBayes::inferALL] ------ inferingALL (nIter=" << nIter << ") ------"<<endl;
  	cout <<"initial labels:"<< endl;
  	cout<<this->labels().transpose()<<endl;
  }
  for(uint32_t t=0; t<nIter; ++t)
  {
    this->sampleLabels();
    this->sampleParameters();
    if(verbose){
		cout << "[" << std::setw(3)<< std::setfill('0')  << t <<"] label: " 
    	<< this->labels().transpose()
      	<< " [joint= " << std::setw(6) << this->logJoint(false) << "]"<< endl;
    }
  }
}
