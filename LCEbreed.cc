/**  $Id: LCEbreed.cc,v 1.13 2015-07-13 08:20:26 fred Exp $
*
*  @file LCEbreed.cc
*  Nemo2
*
*   Copyright (C) 2006-2015 Frederic Guillaume
*   frederic.guillaume@ieu.uzh.ch
*
*   This file is part of Nemo
*
*   Nemo is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   Nemo is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*  created on @date 07.07.2004
*
*  @author fred
*/
#include <deque>
#include "output.h"
#include "LCEbreed.h"
#include "individual.h"
#include "metapop.h"
#include "Uniform.h"
#include "simenv.h"

/* _/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/ */

//                             LCE_Breed_base/

// ----------------------------------------------------------------------------------------
// LCE_Breed_base
// ----------------------------------------------------------------------------------------
LCE_Breed_base::LCE_Breed_base ()
: LifeCycleEvent("", ""),
_mating_system(0), _mating_proportion(1), _mean_fecundity(0), 
//_growthRates(0),
MatingFuncPtr(0),
DoBreedFuncPtr(0), FecundityFuncPtr(0), CheckMatingConditionFuncPtr(0), GetOffsprgSex(0)
//,GetPatchFecundityFuncPtr(0)
{
  ParamUpdater<LCE_Breed_base> * updater = new ParamUpdater<LCE_Breed_base> (&LCE_Breed_base::setMatingSystem);
  
  add_parameter("mating_system",INT,true,true,1,6, updater);
  add_parameter("mating_proportion",DBL,false,true,0,1,updater);
  add_parameter("mating_males",INT,false,false,0,0, updater);
//  add_parameter("growth_model", INT, false, true, 1, 7, updater);
//  add_parameter("growth_rate", DBL, false, false, 0, 0, updater);
  
  updater = new ParamUpdater< LCE_Breed_base > (&LCE_Breed_base::setFecundity);
  add_parameter("mean_fecundity",DBL,false,false,0,0, updater);
  add_parameter("fecundity_dist_stdev",DBL,false,false,0,0, updater);
  add_parameter("fecundity_distribution",STR,false,false,0,0, updater);
  
  updater = new ParamUpdater< LCE_Breed_base > (&LCE_Breed_base::setSexRatio);
  add_parameter("sex_ratio_mode",STR,false,false,0,0, updater);
}
// ----------------------------------------------------------------------------------------
// LCE_Breed_base::setParameters
// ----------------------------------------------------------------------------------------
bool LCE_Breed_base::setParameters ()
{
  return ( setMatingSystem() && setFecundity() && setSexRatio() );
}
// ----------------------------------------------------------------------------------------
// LCE_Breed_base::setMatingSystem
// ----------------------------------------------------------------------------------------
bool LCE_Breed_base::setMatingSystem ()
{
  _mating_system = (int)this->get_parameter_value("mating_system");
  
  if(get_parameter("mating_proportion")->isSet())
    _mating_proportion = this->get_parameter_value("mating_proportion");
  else
    _mating_proportion = 1;
  
  if(_paramSet->isSet("mating_males"))
    _mating_males = (int)_paramSet->getValue("mating_males");
  else
    _mating_males = 1;
  
  //set the mating functions ptr:
  CheckMatingConditionFuncPtr = &LCE_Breed_base::checkNoSelfing;
  DoBreedFuncPtr = &LCE_Breed_base::breed;
  _do_inherit = true; //is true unless the mating system is cloning
  
  switch(_mating_system) {
    //random mating:
    case 1:
    {
      MatingFuncPtr = &LCE_Breed_base::RandomMating;
      break;
    }
    //polygyny:
    case 2:
    {
      if(_mating_proportion == 1)
        if(_mating_males == 1)
          MatingFuncPtr = &LCE_Breed_base::fullPolyginy;
        else
          MatingFuncPtr = &LCE_Breed_base::fullPolyginy_manyMales;
      else
        if(_mating_males == 1)
          MatingFuncPtr = &LCE_Breed_base::partialPolyginy;
        else
          MatingFuncPtr = &LCE_Breed_base::partialPolyginy_manyMales;
        
      CheckMatingConditionFuncPtr = &LCE_Breed_base::checkPolygyny;
      break;
    }
    //monogamy:
    case 3:
    {
      if(_mating_proportion == 1)
        MatingFuncPtr = &LCE_Breed_base::fullMonoginy;
      else 
        MatingFuncPtr = &LCE_Breed_base::partialMonoginy;
      
      break;
    }
    //selfing:
    case 4:
    {
      if(_mating_proportion == 1)
        MatingFuncPtr = &LCE_Breed_base::fullSelfing;
      else
        MatingFuncPtr = &LCE_Breed_base::partialSelfing;
      
      CheckMatingConditionFuncPtr = &LCE_Breed_base::checkSelfing;
      break;
    }
    //cloning
    case 5:
    {
      if(_mating_proportion == 1)
        MatingFuncPtr = &LCE_Breed_base::fullSelfing;
      else
        MatingFuncPtr = &LCE_Breed_base::partialSelfing;
 
      CheckMatingConditionFuncPtr = &LCE_Breed_base::checkCloning;
      DoBreedFuncPtr = &LCE_Breed_base::breed_cloning;
      //      _do_inherit = false;
      break;
    }
    //random mating in Wright-Fisher model with hermaphrodites:
    case 6:
    {
      MatingFuncPtr = &LCE_Breed_base::random_hermaphrodite;
      CheckMatingConditionFuncPtr = &LCE_Breed_base::checkSelfing;
      break;
    }
      
  }
  
  //Growth model
//  unsigned int model;
//  if(_paramSet->isSet("growth_model"))
//    model = get_parameter_value("growth_model");
//  else 
//    model = 1;
//  
//  switch (model) {
//    case 1:
//      GetPatchFecundityFuncPtr = &LCE_Breed_base::instantGrowth;
//      break;
//    case 2:
//      GetPatchFecundityFuncPtr = &LCE_Breed_base::logisticGrowth;
//      break;
//    case 3:
//      GetPatchFecundityFuncPtr = &LCE_Breed_base::stochasticLogisticGrowth;
//      break;
//    case 4:
//      GetPatchFecundityFuncPtr = &LCE_Breed_base::conditionalLogisticGrowth;
//      break;
//    case 5:
//      GetPatchFecundityFuncPtr = &LCE_Breed_base::conditionalStochasticLogisticGrowth;
//      break;
//    case 6:
//      GetPatchFecundityFuncPtr = &LCE_Breed_base::fixedFecundityGrowth;
//      break;
//    case 7:
//      GetPatchFecundityFuncPtr = &LCE_Breed_base::stochasticFecundityGrowth;
//      break;
//    default:
//      GetPatchFecundityFuncPtr = &LCE_Breed_base::instantGrowth;
//      break;
//  }
//  
//  //growth rate
//  if (model > 1 && model < 6) {
//    
//    if(!_paramSet->isSet("growth_rate")) {
//      error("parameter \"growth_rate\" needs to be set\n");
//      return false;
//    }
//    
//    if(_growthRates) delete [] _growthRates;
//    _growthRates = new double [ _popPtr->getPatchNbr() ];
//    
//    if(_paramSet->isMatrix("growth_rate")) {
//      
//      TMatrix tmp;
//      
//      _paramSet->getMatrix("growth_rate", &tmp);
//      
//      if(tmp.getNbCols() != _popPtr->getPatchNbr()){
//        error("matrix argument to \"growth_rate\" has wrong number of elements,\
//              must equal the number of patches.\n");
//        return false;
//      }
//      
//      for (unsigned int i = 0; i < _popPtr->getPatchNbr(); i++) {
//        _growthRates[i] = tmp.get(0, i);
//      }
//      
//      
//    } else { //not a matrix
//      
//      _growthRates[0] = get_parameter_value("growth_rate");
//      
//      for (unsigned int i = 1; i < _popPtr->getPatchNbr(); i++) {
//        _growthRates[i] = _growthRates[0];
//      }
//    }
//    
//    
//  }
  
  return true;
}
// ----------------------------------------------------------------------------------------
// LCE_Breed_base::setSexRatio
// ----------------------------------------------------------------------------------------
bool LCE_Breed_base::setSexRatio ()
{
  // SEX-RATIO
  
  if(get_parameter("sex_ratio_mode")->isSet()) {
    
    if(get_parameter("sex_ratio_mode")->getArg().compare("fixed") == 0) 
  
      GetOffsprgSex = &LCE_Breed_base::getOffsprgSexFixed;
    
    else if(get_parameter("sex_ratio_mode")->getArg().compare("random") != 0) {
    
      error("\"sex_ratio_mode\" parameter argument must be either \"fixed\" or \"random\".");
      return false;
    } else
      GetOffsprgSex = &LCE_Breed_base::getOffsprgSexRandom;
  } else {
    
    switch(_mating_system) {
      case 4: GetOffsprgSex = &LCE_Breed_base::getOffsprgSexSelfing;
        break;
      case 5: GetOffsprgSex = &LCE_Breed_base::getOffsprgSexCloning;
        break;
      case 6: GetOffsprgSex = &LCE_Breed_base::getOffsprgSexSelfing;
        break;
      default: GetOffsprgSex = &LCE_Breed_base::getOffsprgSexRandom;
    }
  }
  return true;
}
// ----------------------------------------------------------------------------------------
// LCE_Breed_base::setFecundity
// ----------------------------------------------------------------------------------------
bool LCE_Breed_base::setFecundity ()
{
  // FECUNDITY
  if (_mean_fecundity) delete _mean_fecundity;
  
  _mean_fecundity = new TMatrix();
  
  if (get_parameter("mean_fecundity")->isMatrix()) {
    
    get_parameter("mean_fecundity")->getMatrix(_mean_fecundity);
    
    if(_mean_fecundity->getNbRows() != 1) {
      error("\"mean_fecundity\" accepts a single number or a single array with patch-specific values.\n");
      return false;
    }
    
    if(_mean_fecundity->getNbCols() > _popPtr->getPatchNbr()) {
      error("\"mean_fecundity\" accepts an array of max num patches in length.\n");
      return false;
    }
    else if (_mean_fecundity->getNbCols() < _popPtr->getPatchNbr()) 
    {
      unsigned int npat = _mean_fecundity->getNbCols();
      
      TMatrix tmp(*_mean_fecundity);
      
      _mean_fecundity->reset(1, _popPtr->getPatchNbr());
      
      for (unsigned int i = 0; i < _popPtr->getPatchNbr(); i++) {
        _mean_fecundity->set(0, i, tmp.get(0 , i % npat));
      }
    }
  } else {
    _mean_fecundity->reset(1, _popPtr->getPatchNbr()); //single array
    _mean_fecundity->assign(get_parameter_value("mean_fecundity"));//will be set to -1 if param isn't set    
  }

  

  if(get_parameter("fecundity_distribution")->isSet()) {
    
    string dist = get_parameter("fecundity_distribution")->getArg();
    
    if( dist.compare("fixed") == 0 )
    
      FecundityFuncPtr = &LCE_Breed_base::getFixedFecundity;
    
    else if( dist.compare("poisson") == 0 )
      
      FecundityFuncPtr = &LCE_Breed_base::getPoissonFecundity;
    
    else if( dist.compare("normal") == 0 ) {
      
      FecundityFuncPtr = &LCE_Breed_base::getGaussianFecundity;
      
      if(get_parameter("fecundity_dist_stdev")->isSet())
        _sd_fecundity = get_parameter_value("fecundity_dist_stdev");
      else {
        error("parameter \"fecundity_dist_stdev\" is missing!\n");
        return false;
      }
      
    } else {
      error("unknown fecundity distribution parameter's argument!\n");
      return false;
    }
  
  } else { //default distribution is Poisson:
    FecundityFuncPtr = &LCE_Breed_base::getPoissonFecundity;
  }
  return true;
}
// ----------------------------------------------------------------------------------------
// LCE_Breed_base::getOffsprgSexFixed
// ----------------------------------------------------------------------------------------
sex_t LCE_Breed_base::getOffsprgSexFixed  ()
{
  static bool sex = RAND::RandBool();
  sex ^= 1;
  return (sex_t)sex;
}

// ----------------------------------------------------------------------------------------
// LCE_Breed_base::makeOffspring
// ----------------------------------------------------------------------------------------
Individual* LCE_Breed_base::makeOffspring(Individual* ind)
{
  unsigned int cat = ind->getPedigreeClass();
  
  ind->getMother()->DidHaveABaby(cat);
  if(cat!=4) ind->getFather()->DidHaveABaby(cat);

  return ind->create(doInheritance(), true);
}
// ----------------------------------------------------------------------------------------
// LCE_Breed_base::breed
// ----------------------------------------------------------------------------------------
Individual* LCE_Breed_base::breed(Individual* mother, Individual* father, unsigned int LocalPatch)
{
  return _popPtr->makeNewIndividual(mother, father, getOffsprgSex(), LocalPatch);
}
// ----------------------------------------------------------------------------------------
// LCE_Breed_base::breed_cloning
// ----------------------------------------------------------------------------------------
Individual* LCE_Breed_base::breed_cloning(Individual* mother, Individual* father, unsigned int LocalPatch)
{
  Individual *newind;
  
  if(mother == father) {
    newind = _popPtr->getNewIndividual();
    //cloning:
    (*newind) = (*mother); 
    
    newind->reset_counters();
    newind->setFather(NULL);
    newind->setFatherID(0);
    newind->setMother(mother);
    newind->setMotherID(mother->getID());
    newind->setIsSelfed(true);
    newind->setHome(LocalPatch);
    newind->setAge(0);
    _do_inherit = false;  
  } else {
    newind =  _popPtr->makeNewIndividual(mother, father, getOffsprgSex(), LocalPatch);
    _do_inherit = true;
  }
  
  return newind;
}
// ----------------------------------------------------------------------------------------

//                             LCE_Breed

// ----------------------------------------------------------------------------------------
// LCE_Breed::init
// ----------------------------------------------------------------------------------------
bool LCE_Breed::setParameters ( )
{ 
  return LCE_Breed_base::setParameters( ); 
}
// ----------------------------------------------------------------------------------------
// LCE_Breed::execute
// ----------------------------------------------------------------------------------------
void LCE_Breed::execute()
{
  Patch* patch;
  Individual* mother;
  Individual* father;
  Individual* NewOffsprg;
  unsigned int nbBaby;
#ifdef _DEBUG_
  message("LCE_Breed::execute (Patch nb: %i offsprg nb: %i adlt nb: %i)\n"
          ,_popPtr->getPatchNbr(),_popPtr->size( OFFSPRG ),_popPtr->size( ADULTS ));
#endif
  //cout << "\nStart of LCE_Breed::execute() !!!!\t";
  
  if(_popPtr->size(OFFSPRG) != 0) {
    warning("offspring containers not empty at time of breeding, flushing.\n");
    _popPtr->flush(OFFSx);
  }
  //because mean fecundity can be patch-specific, we have to check whether the patch number changed
  if (_mean_fecundity->getNbCols() != _popPtr->getPatchNbr()) LCE_Breed_base::setFecundity();
  
  for(unsigned int i = 0; i < _popPtr->getPatchNbr(); i++) {
    
    patch = _popPtr->getPatch(i);
    
    if( !checkMatingCondition(patch) ) continue;
    
    unsigned int cnt =0;
    for(unsigned int size = patch->size(FEM, ADLTx), indexOfMother = 0;
        indexOfMother < size;
        indexOfMother++)
    {
      mother = patch->get(FEM, ADLTx, indexOfMother);
      
      nbBaby = (unsigned int)mother->setFecundity( getFecundity(i) ) ; //allows for patch-specific fec
      cnt += nbBaby;
      //-----------------------------------------------------------------------
      while(nbBaby != 0) {
        
        father = this->getFatherPtr(patch, mother, indexOfMother);
        
        NewOffsprg = makeOffspring( do_breed(mother, father, i) );
        
        patch->add(NewOffsprg->getSex(), OFFSx, NewOffsprg);

        nbBaby--;
      }//_END__WHILE
      
    }

  }
  //cout << "\nEnd of LCE_Breed::execute() !!!!\t";

}
