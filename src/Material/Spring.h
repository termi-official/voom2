/*! 
  \file Jacobian.h
  \brief Interface to a material used to compute the ratio between the element reference and current volume
*/

#ifndef _SPRING_H_
#define _SPRING_H_

#include "FilamentMaterial.h"

namespace voom {
  
  class Spring : public FilamentMaterial
  {
  public: 
    // Constructors/destructors:

  Spring(int ID, Real k): FilamentMaterial(ID), _k(k) {}; 
  Spring(Spring* BaseMaterial): 
    FilamentMaterial(BaseMaterial->_matID), _k(BaseMaterial->_k) {};
    
    // Clone
    virtual Spring* clone() const {
      return new Spring(*this);
    }
    
    // Default copy constructor (compiler should already provide exactly this)
    Spring(const Spring & Old): 
    FilamentMaterial(Old._matID), _k(Old._k) {};

    void setMaterialParameters(const Real  & k) {
      _k = k;
    }; 
    void setInternalParameters(const Vector3d & ) {}; 
    void setRegularizationParameters(const vector<Real > &)    {}; // No regularization parameters for Spring

    Real getMaterialParameters() { 
      Real  MatProp;
      MatProp = _k;
      return MatProp;
    }
    Real getInternalParameters() {
      Real IntParam;
      return IntParam;
    }

    vector<Real> getRegularizationParameters() { // No regularization parameters for Spring
      vector<Real > RegParam;
      return RegParam;
    }

    
    // Operators
    //! Based on current length d, calculates state of the spring
    void compute(Filresults & R,  Vector3d & d, const  Vector3d & d0);
    
    //! Tells if material has history variables 
    // It is used in the Model derived classes
    bool HasHistoryVariables() { return false; };
    
  private:
    Real _k;
    
  }; // class Spring
  
}
#endif // _SPRING_H_
