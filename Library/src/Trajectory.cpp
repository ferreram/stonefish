//
//  Trajectory.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 25/05/2014.
//  Copyright (c) 2014-2017 Patryk Cieslak. All rights reserved.
//

#include "Trajectory.h"

Trajectory::Trajectory(std::string uniqueName, SolidEntity* attachment, btTransform relativeFrame, btScalar frequency, unsigned int historyLength) : SimpleSensor(uniqueName, frequency, historyLength)
{
    solid = attachment;
    relToSolid = UnitSystem::SetTransform(relativeFrame);
    channels.push_back(SensorChannel("Coordinate X", QUANTITY_LENGTH));
    channels.push_back(SensorChannel("Coordinate Y", QUANTITY_LENGTH));
    channels.push_back(SensorChannel("Coordinate Z", QUANTITY_LENGTH));
    channels.push_back(SensorChannel("Roll", QUANTITY_ANGLE));
    channels.push_back(SensorChannel("Pitch", QUANTITY_ANGLE));
    channels.push_back(SensorChannel("Yaw", QUANTITY_ANGLE));
}

void Trajectory::Reset()
{
    SimpleSensor::Reset();
}

void Trajectory::InternalUpdate(btScalar dt)
{
    //calculate transformation from global to imu frame
    btTransform globalFrame = solid->getTransform() * relToSolid;
    
    //angles
    btScalar yaw, pitch, roll;
    globalFrame.getBasis().getEulerYPR(yaw, pitch, roll);
    
    //save sample
    btScalar values[6] = {globalFrame.getOrigin().x(), globalFrame.getOrigin().y(), globalFrame.getOrigin().z(), roll, pitch, yaw};
    Sample s(6, values);
    AddSampleToHistory(s);
}

void Trajectory::Render()
{
    if(history.size() > 1)
	{
		std::vector<glm::vec3> vertices;
		
		for(unsigned int i = 0; i < history.size(); ++i)
            vertices.push_back(glm::vec3(history[i]->getValue(0), history[i]->getValue(1), history[i]->getValue(2)));
    
		OpenGLContent::getInstance()->DrawPrimitives(PrimitiveType::LINE_STRIP, vertices, CONTACT_COLOR);
	}
}

