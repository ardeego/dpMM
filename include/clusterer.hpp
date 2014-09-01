#pragma once


template<class T>
class Clusterer
{
public:
  Clusterer(const boost::shared_ptr<Matrix<T,Dynamic,Dynamic> >& spx, uint32_t K,
    boost::mt19937* pRndGen);
  virtual ~Clusterer();

//  void initialize(const Matrix<T,Dynamic,Dynamic>& x);

  virtual void updateLabels() = 0;
  virtual void updateCenters() = 0;
  virtual MatrixXu mostLikelyInds(uint32_t n, 
      Matrix<T,Dynamic,Dynamic>& deviates) = 0;
  virtual T avgIntraClusterDeviation() = 0;
  
  const VectorXu& z() const {return z_;};
  const Matrix<T,Dynamic,Dynamic>& centroids() const {return ps_;};

protected:
  const uint32_t K_;
  const uint32_t D_;
  const uint32_t N_;
  boost::shared_ptr<Matrix<T,Dynamic,Dynamic> > spx_; // pointer to data
  Matrix<T,Dynamic,Dynamic> ps_; // centroids on the sphere
  VectorXu z_; // labels
  boost::mt19937* pRndGen_;
};

// ----------------------------- impl -----------------------------------------

template<class T>
Clusterer<T>::Clusterer(
    const boost::shared_ptr<Matrix<T,Dynamic,Dynamic> >& spx, uint32_t K,
    boost::mt19937* pRndGen)
  : K_(K), D_(spx->rows()), N_(spx->cols()), spx_(spx), ps_(D_,K_), z_(N_), 
    pRndGen_(pRndGen)
{};

template<class T>
Clusterer<T>::~Clusterer()
{};
