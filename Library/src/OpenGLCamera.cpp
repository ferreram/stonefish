//
//  OpenGLCamera.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 12/12/12.
//  Copyright (c) 2012 Patryk Cieslak. All rights reserved.
//

#include "OpenGLCamera.h"
#include "GeometryUtil.hpp"

OpenGLCamera::OpenGLCamera(const btVector3& eyePosition, const btVector3& targetPosition, const btVector3& cameraUp, GLint x, GLint y, GLint width, GLint height, GLfloat fov, GLfloat horizon, bool sao) : OpenGLView(x, y, width, height, horizon, sao)
{
    eye = UnitSystem::SetPosition(eyePosition);
    dir = UnitSystem::SetPosition(targetPosition - eyePosition);
    dir.normalize();
    lookingDir = dir;
    up = cameraUp.normalized();
    pan = 0;
    tilt = 0;
    fovx = UnitSystem::SetAngle(fov);
    
    holdingEntity = NULL;
    
    GLfloat aspect = (GLfloat)viewportWidth/(GLfloat)viewportHeight;
    GLfloat fovy = fovx/aspect;
    projection = glm::perspective(fovy, aspect, near, far);
    
    SetupCamera();
}

OpenGLCamera::~OpenGLCamera()
{
    holdingEntity = NULL;
}

ViewType OpenGLCamera::getType()
{
    return CAMERA;
}

void OpenGLCamera::MoveCamera(const btVector3& move)
{
    eye = eye + UnitSystem::SetPosition(move);
    SetupCamera();
}

void OpenGLCamera::MoveCamera(btScalar step)
{
    eye = eye + lookingDir * UnitSystem::SetLength(step);
    SetupCamera();
}
void OpenGLCamera::RotateCamera(btScalar panStep, btScalar tiltStep)
{
    pan += UnitSystem::SetAngle(panStep);
    tilt += UnitSystem::SetAngle(tiltStep);
    SetupCamera();
}

void OpenGLCamera::setPanAngle(GLfloat newPanAngle)
{
    newPanAngle = UnitSystem::SetAngle(newPanAngle);
    pan = newPanAngle;
    SetupCamera();
}

void OpenGLCamera::setTiltAngle(GLfloat newTiltAngle)
{
    newTiltAngle = UnitSystem::SetAngle(newTiltAngle);
    tilt = newTiltAngle;
    SetupCamera();
}


GLfloat OpenGLCamera::getPanAngle()
{
    return UnitSystem::GetAngle(pan);
}

GLfloat OpenGLCamera::getTiltAngle()
{
    return UnitSystem::GetAngle(tilt);
}

btVector3 OpenGLCamera::GetEyePosition()
{
    if(holdingEntity != NULL)
    {
        btVector3 newEye =  holdingEntity->getTransform().getBasis() * eye + holdingEntity->getTransform().getOrigin();
        return newEye;
    }
    else
        return eye;
}

btVector3 OpenGLCamera::GetLookingDirection()
{
    if(holdingEntity != NULL)
    {
        btVector3 newDir =  holdingEntity->getTransform().getBasis() * lookingDir;
        return newDir.normalized();
    }
    else
        return lookingDir;
}

btVector3 OpenGLCamera::GetUpDirection()
{
    if(holdingEntity != NULL)
    {
        btVector3 newUp = holdingEntity->getTransform().getBasis() * up;
        return newUp.normalized();
    }
    else
        return up;
}

void OpenGLCamera::GlueToEntity(SolidEntity *ent)
{
    holdingEntity = ent;
}

void OpenGLCamera::SetupCamera()
{
    lookingDir = dir;
    
    //additional camera rotation
    btVector3 tiltAxis = dir.cross(up);
    tiltAxis = tiltAxis.normalize();
    
    btVector3 panAxis = tiltAxis.cross(dir);
    panAxis = panAxis.normalize();
    
    //rotate
    lookingDir = lookingDir.rotate(tiltAxis, tilt);
    lookingDir = lookingDir.rotate(panAxis, pan);
    lookingDir = lookingDir.normalize();
    
    btVector3 newUp = panAxis.rotate(tiltAxis, tilt);
    newUp = newUp.rotate(panAxis, pan);
    newUp = newUp.normalize();
    
#ifdef BT_USE_DOUBLE_PRECISION
    glm::dvec3 eyeV(eye.x(), eye.y(), eye.z());
    glm::dvec3 dirV(lookingDir.x(), lookingDir.y(), lookingDir.z());
    glm::dvec3 upV(newUp.x(), newUp.y(), newUp.z());
    glm::dmat4x4 cameraM = glm::lookAt(eyeV, eyeV+dirV, upV);
#else
    glm::vec3 eyeV(eye.x(), eye.y(), eye.z());
    glm::vec3 dirV(lookingDir.x(), lookingDir.y(), lookingDir.z());
    glm::vec3 upV(newUp.x(), newUp.y(), newUp.z());
    glm::mat4x4 cameraM = glm::lookAt(eyeV, eyeV+dirV, upV);
#endif
    cameraTransform.setFromOpenGLMatrix(glm::value_ptr(cameraM));
    cameraRender = cameraTransform.inverse();
}

btTransform OpenGLCamera::GetViewTransform()
{
    if(holdingEntity != NULL)
    {
        btTransform entTrans = holdingEntity->getTransform();
        btTransform trans =  cameraTransform * entTrans.inverse();
        btVector3 translate = entTrans.getBasis() * eye;
        trans.getOrigin() = trans.getOrigin() - translate;
        return trans;
    }
    else
    {
        btTransform trans = cameraTransform;
        return trans;
    }
}

void OpenGLCamera::RenderDummy()
{
    glm::mat4 model;
	
    //transformation
    if(holdingEntity != NULL)
    {
        btTransform trans = holdingEntity->getTransform();
		model = glMatrixFromBtTransform(trans);
    }
    
    model = glm::translate(model, glm::vec3((GLfloat)eye.x(), (GLfloat)eye.y(), (GLfloat)eye.z()));
	model *= glMatrixFromBtTransform(cameraRender);
   
    //rendering
    GLfloat iconSize = 5.f;
    GLfloat x = iconSize*tanf(fovx/2.f);
    GLfloat aspect = (GLfloat)viewportWidth/(GLfloat)viewportHeight;
    GLfloat y = x/aspect;
	
	std::vector<glm::vec3> vertices;
	vertices.push_back(glm::vec3(0,0,0));
	vertices.push_back(glm::vec3(-x,y,-iconSize));
	vertices.push_back(glm::vec3(0,0,0));
	vertices.push_back(glm::vec3(x,y,-iconSize));
	vertices.push_back(glm::vec3(0,0,0));
	vertices.push_back(glm::vec3(-x,-y,-iconSize));
	vertices.push_back(glm::vec3(0,0,0));
	vertices.push_back(glm::vec3(x,-y,-iconSize));
	
	vertices.push_back(glm::vec3(-x,y,-iconSize));
	vertices.push_back(glm::vec3(x,y,-iconSize));
	vertices.push_back(glm::vec3(x,y,-iconSize));
	vertices.push_back(glm::vec3(x,-y,-iconSize));
	vertices.push_back(glm::vec3(x,-y,-iconSize));
	vertices.push_back(glm::vec3(-x,-y,-iconSize));
	vertices.push_back(glm::vec3(-x,-y,-iconSize));
	vertices.push_back(glm::vec3(-x,y,-iconSize));
	OpenGLContent::getInstance()->DrawPrimitives(PrimitiveType::LINES, vertices, DUMMY_COLOR, model);
}
