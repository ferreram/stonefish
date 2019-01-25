//
//  RevoluteJoint.h
//  Stonefish
//
//  Created by Patryk Cieslak on 1/13/13.
//  Copyright (c) 2013-2019 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_RevoluteJoint__
#define __Stonefish_RevoluteJoint__

#include "joints/Joint.h"

namespace sf
{
    class SolidEntity;
    
    //! A class representing a revolute joint.
    class RevoluteJoint : public Joint
    {
    public:
        //! A constructor (a revolute joint between two solid bodies).
        /*!
         \param uniqueName a name for the revolute joint
         \param solidA pointer to the first solid body
         \param solidB pointer to the second solid body
         \param pivot a point where the axis of joint is located
         \param axis a vector parallel to the joint axis
         \param collideLinked a flag that sets if the bodies connected by the joint should coliide
        */
        RevoluteJoint(std::string uniqueName, SolidEntity* solidA, SolidEntity* solidB,
                      const Vector3& pivot, const Vector3& axis, bool collideLinked = true);
        
        //! A constructor (a revolute joint attaching a solid to the world).
        /*!
         \param uniqueName a name for the revolute joint
         \param solid pointer to the solid body
         \param pivot a point where the axis of joint is located
         \param axis a vector parallel to the joint axis
         */
        RevoluteJoint(std::string uniqueName, SolidEntity* solid, const Vector3& pivot, const Vector3& axis);
        
        //! A method used to apply torque to the joint.
        /*!
         \param T torque to be applied to the joint [Nm]
         */
        void ApplyTorque(Scalar T);
        
        //! A method applying damping to the joint.
        void ApplyDamping();
        
        //! A method that solves initial conditions problem for the joint.
        /*!
         \param linearTolerance a value of the tolerance in position (termination condition)
         \param angularTolerance a value of the tolerance in rotation (termination condition)
         */
        bool SolvePositionIC(Scalar linearTolerance, Scalar angularTolerance);
        
        //! A method implementing the rendering of the joint.
        std::vector<Renderable> Render();
        
        //! A method to set the damping characteristics of the joint.
        /*!
         \param constantFactor a constant damping torque [Nm]
         \param viscousFactor a coefficient of viscous damping [Nm*s*rad^-1]
         */
        void setDamping(Scalar constantFactor, Scalar viscousFactor);
        
        //! A method to set the limits of the joint.
        /*!
         \param min the minimum rotation angle of the joint [rad]
         \param max the maximum rotation angle of the joint [rad]
         */
        void setLimits(Scalar min, Scalar max);
        
        //! A method to set the desired initial condition of the joint.
        /*!
         \param angle the initial angle of the joint [rad]
         */
        void setIC(Scalar angle);
        
        //! A method returning the anglular position of the joint [rad].
        Scalar getAngle();
        
        //! A method returning the angular velocity of the joint [rad*s^-1]/
        Scalar getAngularVelocity();
        
        //! A method returning the type of the joint.
        JointType getType();
        
    private:
        Vector3 axisInA;
        Vector3 pivotInA;
        Scalar sigDamping;
        Scalar velDamping;
        Scalar angleIC;
    };
}

#endif