/*! 
  \file Jacobian.h
  \brief Interface to a material used to compute the ratio between the element reference and current volume
*/

#ifndef _SPRING_H_
#define _SPRING_H_

#include "MechanicsMaterial.h"

namespace voom {
  
  class Spring : public MechanicsMaterial
  {
  public: 
    // Constructors/destructors:
  Spring(int ID): MechanicsMaterial(ID), _k(1.0), _d0(0.0) {};
  Spring(int ID, Real k, Vector3D d0): MechanicsMaterial(ID), _k(k), _d0(d0) {}; 
  Spring(Spring* BaseMaterial): 
    MechanicsMaterial(BaseMaterial->_matID), _k(BaseMaterial->_k), _d0(BaseMaterial->_d0) {};
    
    // Clone
    virtual Spring* clone() const {
      return new Spring(*this);
    }
    
    // Default copy constructor (compiler should already provide exactly this)
    Spring(const Spring & Old): 
    MechanicsMaterial(Old._matID), _k(Old._k),_d0(Old._d0) {};

    void setMaterialParameters(const vector<Real > & k) {
      _k = k;
    }; 
    void setInternalParameters(const vector<Vector3D > & d0) {
      _d0 = d0
    }; 
    Void setRegularizationParameters(const vector<Real > &)    {}; // No regularization parameters for Spring

    vector<Real > getMaterialParameters() { 
      vector<Real > MatProp(1,0.0);
      MatProp[0] = _k;
      return MatProp;
    }
    vector<Real > getInternalParameters() { 
      vector<Real > IntParam(1,0.0);
      IntParam[0] = _d0
      return IntParam;
    }
    vector<Real > getRegularizationParameters() { // No regularization parameters for Spring
      vector<Real > RegParam;
      return RegParam;
    }

    
    // Operators
    //! Based on current length d, calculates state of the spring
    void compute(Filresults & R, const Vector3D & d);

    //! Tells if material has history variables 
    // It is used in the Model derived classes
    bool HasHistoryVariables() { return false; };
    
  private:
	
  }; // class Spring
  
}
#endif // _SPRING_H_