//
//  GearJoint.h
//  Stonefish
//
//  Created by Patryk Cieslak on 28/03/2014.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_GearJoint__
#define __Stonefish_GearJoint__

#include "Joint.h"

class GearJoint : public Joint
{
public:
    GearJoint(std::string uniqueName, SolidEntity* solidA, SolidEntity* solidB, const btVector3& axisA, const btVector3& axisB, btScalar ratio);
    ~GearJoint();
    
    void Render();
    JointType getType();
    btScalar getRatio();
    
private:
    btScalar gearRatio;
};

#endif