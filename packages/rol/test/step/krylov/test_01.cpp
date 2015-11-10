// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

/*! \file  test_01.cpp
    \brief Test GMRES solver
*/

#include "ROL_StdVector.hpp"
#include "ROL_GMRES.hpp"
#include "ROL_RandomVector.hpp"

#include "Teuchos_oblackholestream.hpp"
#include "Teuchos_GlobalMPISession.hpp"

#include<iomanip>

// Identity operator for preconditioner
template<class Real> 
class Identity : public ROL::LinearOperator<Real> {
  typedef ROL::Vector<Real> V;
public:
  void apply( V& Hv, const V& v, Real &tol ) const { 
    Hv.set(v); 
  }
}; // class Identity


// Apply a tridiagonal Toeplitz matrix to a ROL::StdVector to test Krylov solvers
template<class Real>
class TridiagonalToeplitzOperator : public ROL::LinearOperator<Real> {

  typedef std::vector<Real>    vector;
  typedef ROL::Vector<Real>    V;
  typedef ROL::StdVector<Real> SV;

  typedef typename vector::size_type uint; 

private:

  Real a_; // subdiagonal
  Real b_; // diagonal 
  Real c_; // superdiagonal

  Teuchos::LAPACK<int,Real> lapack_;

public:

  TridiagonalToeplitzOperator( Real &a, Real &b, Real &c ) : a_(a), b_(b), c_(c) {}

  // Tridiagonal multiplication
  void apply( V &Hv, const V &v, Real &tol ) const {

    using Teuchos::RCP;  using Teuchos::dyn_cast;
 
    SV &Hvs = dyn_cast<SV>(Hv);
    RCP<vector> Hvp = Hvs.getVector();
 
    const SV &vs = dyn_cast<const SV>(v);
    RCP<const vector> vp = vs.getVector();

    uint n = vp->size();

    (*Hvp)[0] = b_*(*vp)[0] + c_*(*vp)[1];

    for(uint k=1; k<n-1; ++k) {
      (*Hvp)[k]  = a_*(*vp)[k-1] + b_*(*vp)[k] + c_*(*vp)[k+1];  
    } 
  
    (*Hvp)[n-1] = a_*(*vp)[n-2] + b_*(*vp)[n-1];

  }

  // Tridiagonal solve - compare against GMRES
  void applyInverse( V &Hv, const V &v, Real &tol ) const {

    using Teuchos::RCP;  using Teuchos::dyn_cast;
 
    SV &Hvs = dyn_cast<SV>(Hv);
    RCP<vector> Hvp = Hvs.getVector();
 
    const SV &vs = dyn_cast<const SV>(v);
    RCP<const vector> vp = vs.getVector();

    uint n = vp->size();

    const char TRANS = 'N';
    const int NRHS = 1;

    vector dl(n-1,a_);
    vector d(n,b_);
    vector du(n-1,c_);
    vector du2(n-2,0.0);
 
    std::vector<int> ipiv(n);
    int info;

    Hv.set(v); // LAPACK will modify this in place    

    // Do Tridiagonal LU factorization
    lapack_.GTTRF(n,&dl[0],&d[0],&du[0],&du2[0],&ipiv[0],&info);

    // Solve the system with the LU factors
    lapack_.GTTRS(TRANS,n,NRHS,&dl[0],&d[0],&du[0],&du2[0],&ipiv[0],&(*Hvp)[0],n,&info);

  }

}; // class TridiagonalToeplitzOperator



typedef double RealT;

int main(int argc, char *argv[]) {

  using Teuchos::RCP;
  using Teuchos::rcp; 

  typedef std::vector<RealT>            vector;
  typedef ROL::StdVector<RealT>         SV; 

  typedef typename vector::size_type    uint;

  Teuchos::GlobalMPISession mpiSession(&argc, &argv);

  int iprint     = argc - 1;
  RCP<std::ostream> outStream;
  Teuchos::oblackholestream bhs; // outputs nothing
  if (iprint > 0)
    outStream = rcp(&std::cout, false);
  else
    outStream = rcp(&bhs, false);

  int errorFlag = 0;

  try {

    Teuchos::ParameterList parlist;

    Teuchos::ParameterList &klist = parlist.sublist("General").sublist("Krylov");

    klist.set("Iteration Limit",20);
    klist.set("Absolute Tolerance",1.e-8);
    klist.set("Relative Tolerance",1.e-6);
    klist.set("Use Initial Guess",false);

    uint dim = 10;

    RCP<vector> xp = rcp( new vector(dim,0.0) );
    RCP<vector> yp = rcp( new vector(dim,0.0) );
    RCP<vector> zp = rcp( new vector(dim,0.0) );
    RCP<vector> bp = rcp( new vector(dim,0.0) );

    SV x(xp); // Exact solution
    SV y(yp); // Solution using direct solve
    SV z(zp); // Solution using GMRES

    SV b(bp); // Right-hand-side    
    
    RealT left = -1.0;
    RealT right = 1.0;    

    ROL::RandomizeVector(x,left,right);
   
    RealT sub   = -1.0;
    RealT diag  =  2.0;
    RealT super = -1.0;

    TridiagonalToeplitzOperator<RealT> T(sub,diag,super);
    Identity<RealT> I;

    RealT tol = 0.0;
      
    T.apply(b,x,tol);

    T.applyInverse(y,b,tol);

    ROL::GMRES<RealT> gmres(parlist);

    int iter;
    int flag;

    gmres.run(z,T,b,I,iter,flag);


    *outStream << std::setw(10) << "Exact"  
               << std::setw(10) << "LAPACK"
               << std::setw(10) << "GMRES " << std::endl;
    *outStream << "---------------------------------" << std::endl;

    for(uint k=0;k<dim;++k) {
      *outStream << std::setw(10) << (*xp)[k] << " " 
                 << std::setw(10) << (*yp)[k] << " "
                 << std::setw(10) << (*zp)[k] << " " << std::endl;
    }  
 
    *outStream << "GMRES performed " << iter << " iterations." << std::endl;

    z.axpy(-1.0,x);

    if( z.norm() > std::sqrt(ROL::ROL_EPSILON) ) {
      ++errorFlag;
    }

  }
  catch (std::logic_error err) {
    *outStream << err.what() << "\n";
    errorFlag = -1000;
  }; // end try

  if (errorFlag != 0)
    std::cout << "End Result: TEST FAILED\n";
  else
    std::cout << "End Result: TEST PASSED\n";

  return 0;   
}
