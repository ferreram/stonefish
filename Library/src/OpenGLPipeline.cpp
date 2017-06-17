//
//  OpenGLPipeline.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 30/03/2014.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#include "OpenGLPipeline.h"

#include "SimulationManager.h"
#include "GLSLShader.h"
#include "GeometryUtil.hpp"
#include "OpenGLGBuffer.h"
#include "OpenGLContent.h"
#include "OpenGLView.h"
#include "OpenGLSky.h"
#include "OpenGLSun.h"
#include "OpenGLLight.h"
#include "Console.h"
#include "PathGenerator.h"
#include "PathFollowingController.h"

OpenGLPipeline* OpenGLPipeline::instance = NULL;

OpenGLPipeline* OpenGLPipeline::getInstance()
{
    if(instance == NULL)
        instance = new OpenGLPipeline();
    
    return instance;
}

OpenGLPipeline::OpenGLPipeline()
{
    renderSky = renderShadows = renderFluid = renderSAO = false;
    showCoordSys = showJoints = showActuators = showSensors = false;
    showLightMeshes = showCameraFrustums = false;
}

OpenGLPipeline::~OpenGLPipeline()
{
    OpenGLView::Destroy();
    OpenGLLight::Destroy();
	OpenGLContent::Destroy();
	
    glDeleteTextures(1, &displayTexture);
    glDeleteFramebuffers(1, &displayFBO);
}

void OpenGLPipeline::setRenderingEffects(bool sky, bool shadows, bool fluid, bool ssao)
{
    renderSky = sky;
    renderShadows = shadows;
    renderFluid = fluid;
    renderSAO = ssao;
}

void OpenGLPipeline::setVisibleHelpers(bool coordSystems, bool joints, bool actuators, bool sensors, bool lights, bool cameras)
{
    showCoordSys = coordSystems;
    showJoints = joints;
    showActuators = actuators;
    showSensors = sensors;
    showLightMeshes = lights;
    showCameraFrustums = cameras;
}

void OpenGLPipeline::setDebugSimulation(bool enabled)
{
    drawDebug = enabled;
}

bool OpenGLPipeline::isFluidRendered()
{
    return renderFluid;
}

bool OpenGLPipeline::isSAORendered()
{
    return renderSAO;
}

GLuint OpenGLPipeline::getDisplayTexture()
{
    return displayTexture;
}

void OpenGLPipeline::Initialize(GLint windowWidth, GLint windowHeight)
{
    windowW = windowWidth;
    windowH = windowHeight;
    
    //Load shaders and create rendering buffers
    cInfo("Loading scene shaders...");
	OpenGLContent::getInstance()->Init();
	OpenGLSky::getInstance()->Init();
    OpenGLSun::getInstance()->Init();
    OpenGLView::Init();
    OpenGLLight::Init();
    
    cInfo("Generating sky...");
    OpenGLSky::getInstance()->Generate(40.f,300.f);
    
    //Set default options
    cInfo("Setting up basic OpenGL parameters...");
    setRenderingEffects(true, true, true, true);
    setVisibleHelpers(false, false, false, false, false, false);
    setDebugSimulation(false);
    
    //OpenGL flags and params
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glShadeModel(GL_SMOOTH);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glPointSize(5.f);
    glLineWidth(1.0f);
    glLineStipple(3, 0xE4E4);
    
    //Create display framebuffer
    glGenFramebuffers(1, &displayFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, displayFBO);
    
    glGenTextures(1, &displayTexture);
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, windowW, windowH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, displayTexture, 0);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
        cError("Display FBO initialization failed!");
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cInfo("OpenGL pipeline initialized.");
}

void OpenGLPipeline::DrawDisplay()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, displayFBO);
    glDrawBuffer(GL_BACK);
    glBlitFramebuffer(0, 0, windowW, windowH, 0, 0, windowW, windowH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void OpenGLPipeline::DrawObjects(SimulationManager* sim)
{
    for(int i=0; i<sim->entities.size(); ++i)
        sim->entities[i]->Render();
}

void OpenGLPipeline::Render(SimulationManager* sim)
{
    //Clear display framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, displayFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    //Render shadow maps (these do not change depending on camera)
    if(renderShadows)
	{
		OpenGLContent::getInstance()->SetDrawFlatObjects(true);
        for(unsigned int i=0; i<sim->lights.size(); ++i)
            sim->lights[i]->RenderShadowMap(this, sim);
	}
    
    //Render all camera views
    for(int i=0; i<sim->views.size(); i++)
    {
        if(sim->views[i]->isActive()) //Render only if active
        {
            GLuint finalTexture = 0;
            
            //Setup and initialize lighting
            OpenGLSun::getInstance()->SetCamera(sim->views[i]);
			OpenGLLight::SetCamera(sim->views[i]);
			
            if(renderShadows)
			{
				OpenGLContent::getInstance()->SetDrawFlatObjects(true);
                OpenGLSun::getInstance()->RenderShadowMaps(this, sim); //This shadow depends on camera frustum
			}
			
            //Setup viewport
            GLint* viewport = sim->views[i]->GetViewport();
            sim->views[i]->SetViewport();
            OpenGLContent::getInstance()->SetViewportSize(viewport[2],viewport[3]);
			
            //Fill plain G-buffer
			OpenGLContent::getInstance()->SetDrawFlatObjects(false);
            sim->views[i]->getGBuffer()->Start(0);
            sim->views[i]->SetProjection();
            sim->views[i]->SetViewTransform();
            DrawObjects(sim);
            sim->views[i]->getGBuffer()->Stop();
            
            //Prepare SAO
            if(renderSAO && sim->views[i]->hasSSAO())
            {
                glActiveTexture(GL_TEXTURE0);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getNormalsTexture(0));
                
                glActiveTexture(GL_TEXTURE1);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, OpenGLView::getRandomTexture());
    
                glActiveTexture(GL_TEXTURE2);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getPositionTexture(0));
                
                OpenGLView::SetTextureUnits(2, 0, 1);
                sim->views[i]->RenderSSAO();
            }
            
            
            //if(!renderFluid || sim->ocean == NULL)
            /////////////////////////////// N O R M A L  P I P E L I N E //////////////////////////////
            {
            normal_pipeline:
                glBindFramebuffer(GL_FRAMEBUFFER, sim->views[i]->getSceneFBO());
                glDrawBuffer(SCENE_ATTACHMENT);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                
                //1. Create stencil mask - optimize rendering where occluded
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_STENCIL_TEST);
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                glDisable(GL_BLEND);
                
                glStencilFunc(GL_ALWAYS, 1, 0xFF);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                glStencilMask(0xFF);
                glClear(GL_STENCIL_BUFFER_BIT);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                
				OpenGLContent::getInstance()->SetDrawFlatObjects(true);
                DrawObjects(sim);
				
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glStencilMask(0x00);
                
                //2. Enter deferred rendering
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                
                //3. Draw sky where stencil = 0
                if(renderSky)
                {
                    glStencilFunc(GL_EQUAL, 0, 0xFF);
                    OpenGLSky::getInstance()->Render(sim->views[i], sim->views[i]->GetViewTransform(), sim->zUp);
                }
                
                //4. Bind deferred textures to texture units
                glStencilFunc(GL_EQUAL, 1, 0xFF);
                
                glActiveTexture(GL_TEXTURE0);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getDiffuseTexture()); //Diffuse is reused
                
                glActiveTexture(GL_TEXTURE1);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getPositionTexture(0));
                
                glActiveTexture(GL_TEXTURE2);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getNormalsTexture(0));
                
                glActiveTexture(GL_TEXTURE3);
                glEnable(GL_TEXTURE_CUBE_MAP);
                glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getDiffuseCubemap());
                
                glActiveTexture(GL_TEXTURE4);
                glEnable(GL_TEXTURE_CUBE_MAP);
                glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getReflectionCubemap());
                
                //5. Bind SSAO texture if needed
                glActiveTexture(GL_TEXTURE5);
                glEnable(GL_TEXTURE_2D);
                if(renderSAO && sim->views[i]->hasSSAO())
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getSSAOTexture());
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
                
                OpenGLSun::getInstance()->SetTextureUnits(0, 2, 1, 6);
                OpenGLLight::SetTextureUnits(0, 2, 1, 3, 4, 5, 6);
                
                //5. Render ambient pass - sky, ssao
				if(renderSky)
					OpenGLLight::RenderAmbientLight(sim->views[i]->GetViewTransform(), sim->zUp);
                
                //6. Render lights
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_ONE, GL_ONE); //accumulate light
                
                OpenGLSun::getInstance()->Render(sim->views[i]->GetViewTransform()); //Render sun pass
                
                for(int h=0; h<sim->lights.size(); h++) //Render light passes
                    sim->lights[h]->Render();
                
                //7. Reset OpenGL
                glUseProgram(0);
                glDisable(GL_STENCIL_TEST);
                
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, 0);
                
                glActiveTexture(GL_TEXTURE1);
                glDisable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);
                
                glActiveTexture(GL_TEXTURE2);
                glDisable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);
                
                glActiveTexture(GL_TEXTURE3);
                glDisable(GL_TEXTURE_CUBE_MAP);
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                glDisable(GL_TEXTURE_CUBE_MAP);
                
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
                
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
                
                //8. Finish rendering
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                finalTexture = sim->views[i]->getSceneTexture();
			}
		
            ///////////FINAL TONEMAPPED/DISTORTED RENDER///////
			glEnable(GL_SCISSOR_TEST);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            
            glScissor(viewport[0], viewport[1], viewport[2], viewport[3]);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            sim->views[i]->RenderHDR(displayFBO);
           
            /////////OVERLAY DUMMIES////////
            glBindFramebuffer(GL_FRAMEBUFFER, displayFBO);
			glBindTexture(GL_TEXTURE_2D, 0);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_DEPTH_BUFFER_BIT);
			
            glm::mat4 proj = sim->views[i]->GetProjectionMatrix();
			glm::mat4 view = sim->views[i]->GetViewMatrix(sim->views[i]->GetViewTransform());
			OpenGLContent::getInstance()->SetProjectionMatrix(proj);
			OpenGLContent::getInstance()->SetViewMatrix(view);
			
            //Bullet debug draw
            if(drawDebug)
                sim->dynamicsWorld->debugDrawWorld();
            
            //Coordinate systems
            if(showCoordSys)
            {
				OpenGLContent::getInstance()->DrawCoordSystem(glm::mat4(), 2.f);
                
                for(int h = 0; h < sim->entities.size(); h++)
                    if(sim->entities[h]->getType() == ENTITY_SOLID)
                    {
						SolidEntity* solid = (SolidEntity*)sim->entities[h];
                        btTransform comT = solid->getTransform();
                        OpenGLContent::getInstance()->DrawCoordSystem(glMatrixFromBtTransform(comT), 0.1f);
                    }
                    else if(sim->entities[h]->getType() == ENTITY_FEATHERSTONE)
                    {
                        FeatherstoneEntity* fe = (FeatherstoneEntity*)sim->entities[h];
                        fe->RenderStructure();
                    }
                    else if(sim->entities[h]->getType() == ENTITY_SYSTEM)
                    {
                        SystemEntity* system = (SystemEntity*)sim->entities[h];
                        btTransform comT = system->getTransform();
                        OpenGLContent::getInstance()->DrawCoordSystem(glMatrixFromBtTransform(comT), 0.1f);
                    }
            }
            
            //Joints
			for(int h=0; h<sim->joints.size(); h++)
				if(sim->joints[h]->isRenderable())
					sim->joints[h]->Render();
            
            //Contact points
            for(int h = 0; h < sim->contacts.size(); h++)
                sim->contacts[h]->Render();
            
            //Sensors
            for(int h = 0; h < sim->sensors.size(); h++)
                if(sim->sensors[h]->isRenderable())
                    sim->sensors[h]->Render();
            
            //Paths
            for(int h = 0; h < sim->controllers.size(); h++)
                if(sim->controllers[h]->getType() == CONTROLLER_PATHFOLLOWING)
                    ((PathFollowingController*)sim->controllers[h])->RenderPath();

            //Lights
            if(showLightMeshes)
                for(int h = 0; h < sim->lights.size(); h++)
                    sim->lights[h]->RenderDummy();
            
            //Cameras
            if(showCameraFrustums)
                for(int h = 0; h < sim->views.size(); h++)
                    if(i != h && sim->views[h]->getType() == CAMERA)
                    {
                        OpenGLCamera* cam = (OpenGLCamera*)sim->views[h];
                        cam->RenderDummy();
                    }
            
            glDisable(GL_SCISSOR_TEST);
            
            //Debugging
			//sim->views[i]->getGBuffer()->ShowTexture(DIFFUSE, 0,0,300,200); // FBO debugging
            //sim->views[i]->getGBuffer()->ShowTexture(POSITION1,0,200,300,200); // FBO debugging
			//sim->views[i]->getGBuffer()->ShowTexture(NORMAL1,0,400,300,200); // FBO debugging
			
			//sim->views[i]->ShowSceneTexture(NORMAL, 0, 600, 300, 200);
            //sim->lights[0]->ShowShadowMap(0, 800, 300, 300);
			
			//OpenGLSky::getInstance()->ShowCubemap(SKY);
 			
			//sim->views[i]->ShowAmbientOcclusion(0, 0, 300, 200);		
            
			//OpenGLSun::getInstance()->ShowShadowMaps(0, 0, 0.2);
           
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
            delete viewport;
        }
    }
}



/*			
			
            /////////////////////////////////////////////////////////////////////////////////////
            else
            {
                btVector3 normal, position;
                sim->ocean->GetSurface(normal, position);
                bool underwater = distanceFromCenteredPlane(normal, position - sim->views[i]->GetEyePosition()) >= 0;
                
                if(underwater) //Under the surface
                {
                    bool inside = sim->ocean->IsInsideFluid(sim->views[i]->GetEyePosition()
                                                                   + sim->views[i]->GetLookingDirection() * sim->views[i]->GetNearClip());
                    if(inside)
            ////////////////////// U N D E R W A T E R  P I P E L I N E /////////////////////////
                    {
                        btScalar openglTrans[16];
                        
                        /////////// Render normal scene with special stencil masking ////////////////
                        {
                        glBindFramebuffer(GL_FRAMEBUFFER, sim->views[i]->getSceneFBO());
                        glDrawBuffer(SCENE_ATTACHMENT);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    
                        //1. Create stencil mask
                        glEnable(GL_DEPTH_TEST);
                        glEnable(GL_STENCIL_TEST);
                        glEnable(GL_CULL_FACE);
                        glCullFace(GL_BACK);
                        glDisable(GL_BLEND);
                    
                        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                        glStencilMask(0xFF);
                        glClear(GL_STENCIL_BUFFER_BIT);
                    
                        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                        sim->views[i]->SetProjection();
                        sim->views[i]->GetViewTransform().getOpenGLMatrix(openglTrans);
                        glMatrixMode(GL_MODELVIEW);
#ifdef BT_USE_DOUBLE_PRECISION
                        glLoadMatrixd(openglTrans);
#else
                        glLoadMatrixf(openglTrans);
#endif
                        //Render normal objects and set stencil to 1
                        glStencilFunc(GL_ALWAYS, 1, 0xFF);
                        DrawObjects(sim);
                        //Render fluid surface and change stencil to 2
                        glStencilFunc(GL_ALWAYS, 2, 0xFF);
                        sim->ocean->RenderSurface();
                    
                        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        glStencilMask(0x00);
                    
                        //2. Enter deferred rendering
                        glDisable(GL_DEPTH_TEST);
                        glDisable(GL_CULL_FACE);
                    
                        OpenGLContent::getInstance()->SetupOrtho();
                    
                        //3. Draw sky where stencil = 0
                        if(renderSky)
                        {
                            glStencilFunc(GL_EQUAL, 0, 0xFF);
                            OpenGLSky::getInstance()->Render(sim->views[i], sim->views[i]->GetViewTransform(), sim->zUp);
                        }
                    
                        //4. Bind deferred textures to texture units
                        glStencilFunc(GL_EQUAL, 1, 0xFF);
                    
                        glActiveTexture(GL_TEXTURE0);
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getDiffuseTexture()); //Diffuse is reused
                    
                        glActiveTexture(GL_TEXTURE1);
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getPositionTexture(0));
                    
                        glActiveTexture(GL_TEXTURE2);
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getNormalsTexture(0));
                    
                        glActiveTexture(GL_TEXTURE3);
                        glEnable(GL_TEXTURE_CUBE_MAP);
                        glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getDiffuseCubemap());
                    
                        glActiveTexture(GL_TEXTURE4);
                        glEnable(GL_TEXTURE_CUBE_MAP);
                        glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getReflectionCubemap());
                    
                        //5. Bind SSAO texture if needed
                        glActiveTexture(GL_TEXTURE5);
                        glEnable(GL_TEXTURE_2D);
                        if(renderSAO && sim->views[i]->hasSSAO())
                            glBindTexture(GL_TEXTURE_2D, sim->views[i]->getSSAOTexture());
                        else
                            glBindTexture(GL_TEXTURE_2D, 0);
                    
                        OpenGLSun::getInstance()->SetTextureUnits(0, 2, 1, 6);
                        OpenGLLight::SetTextureUnits(0, 2, 1, 3, 4, 5, 6);
                    
                        //5. Render ambient pass - sky, ssao
                        OpenGLLight::RenderAmbientLight(sim->views[i]->GetViewTransform(), sim->zUp);
                    
                        glEnable(GL_BLEND);
                        glBlendEquation(GL_FUNC_ADD);
                        glBlendFunc(GL_ONE, GL_ONE); //accumulate light
                    
                        //6. Render lights
                        OpenGLSun::getInstance()->Render(sim->views[i]->GetViewTransform()); //Render sun pass
                    
                        for(int h=0; h<sim->lights.size(); h++) //Render light passes
                            sim->lights[h]->Render();
                    
                        //7. Reset OpenGL
                        glUseProgramObjectARB(0);
                        glDisable(GL_STENCIL_TEST);
                    
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, 0);
                    
                        glActiveTexture(GL_TEXTURE1);
                        glDisable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, 0);
                    
                        glActiveTexture(GL_TEXTURE2);
                        glDisable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, 0);
                    
                        glActiveTexture(GL_TEXTURE3);
                        glDisable(GL_TEXTURE_CUBE_MAP);
                        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    
                        glActiveTexture(GL_TEXTURE4);
                        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                        glDisable(GL_TEXTURE_CUBE_MAP);
                    
                        glActiveTexture(GL_TEXTURE5);
                        glBindTexture(GL_TEXTURE_2D, 0);
                        glDisable(GL_TEXTURE_2D);
                    
                        glActiveTexture(GL_TEXTURE6);
                        glBindTexture(GL_TEXTURE_2D, 0);
                        glDisable(GL_TEXTURE_2D);
                    
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                        }
                        
                    }
            /////////////////////////////////////////////////////////////////////////////////////
                    else
                        goto normal_pipeline;
                }
                else
            //////////////////////// A B O V E  W A T E R  P I P E L I N E //////////////////////
                {
                    btScalar openglTrans[16];
                    
                    double surface[4];
                    surface[0] = normal.x();
                    surface[1] = normal.y();
                    surface[2] = normal.z();
                    surface[3] = -normal.dot(position);
                    
                    /////////// Render normal scene with special stencil masking ////////////////
                    {
                    glBindFramebuffer(GL_FRAMEBUFFER, sim->views[i]->getSceneFBO());
                    glDrawBuffer(SCENE_ATTACHMENT);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    
                    //1. Create stencil mask
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_STENCIL_TEST);
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    glDisable(GL_BLEND);
                    
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                    glStencilMask(0xFF);
                    glClear(GL_STENCIL_BUFFER_BIT);
                    
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    sim->views[i]->SetProjection();
                    sim->views[i]->GetViewTransform().getOpenGLMatrix(openglTrans);
                    glMatrixMode(GL_MODELVIEW);
#ifdef BT_USE_DOUBLE_PRECISION
                    glLoadMatrixd(openglTrans);
#else
                    glLoadMatrixf(openglTrans);
#endif
                    //Render normal objects and set stencil to 1
                    glStencilFunc(GL_ALWAYS, 1, 0xFF);
                    DrawObjects(sim);
                    //Render fluid surface and change stencil to 2
                    glStencilFunc(GL_ALWAYS, 2, 0xFF);
                    sim->ocean->RenderSurface();
                    
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glStencilMask(0x00);
                    
                    //2. Enter deferred rendering
                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    
                    OpenGLContent::getInstance()->SetupOrtho();
                    
                    //3. Draw sky where stencil = 0
                    if(renderSky)
                    {
                        glStencilFunc(GL_EQUAL, 0, 0xFF);
                        OpenGLSky::getInstance()->Render(sim->views[i], sim->views[i]->GetViewTransform(), sim->zUp);
                    }
                    
                    //4. Bind deferred textures to texture units
                    glStencilFunc(GL_EQUAL, 1, 0xFF);
                    
                    glActiveTexture(GL_TEXTURE0);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getDiffuseTexture()); //Diffuse is reused
                    
                    glActiveTexture(GL_TEXTURE1);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getPositionTexture(0));
                    
                    glActiveTexture(GL_TEXTURE2);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getNormalsTexture(0));
                    
                    glActiveTexture(GL_TEXTURE3);
                    glEnable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getDiffuseCubemap());
                    
                    glActiveTexture(GL_TEXTURE4);
                    glEnable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getReflectionCubemap());
                    
                    //5. Bind SSAO texture if needed
                    glActiveTexture(GL_TEXTURE5);
                    glEnable(GL_TEXTURE_2D);
                    if(renderSAO && sim->views[i]->hasSSAO())
                        glBindTexture(GL_TEXTURE_2D, sim->views[i]->getSSAOTexture());
                    else
                        glBindTexture(GL_TEXTURE_2D, 0);
                    
                    OpenGLSun::getInstance()->SetTextureUnits(0, 2, 1, 6);
                    OpenGLLight::SetTextureUnits(0, 2, 1, 3, 4, 5, 6);
                    
                    //5. Render ambient pass - sky, ssao
                    OpenGLLight::RenderAmbientLight(sim->views[i]->GetViewTransform(), sim->zUp);
                    
                    glEnable(GL_BLEND);
                    glBlendEquation(GL_FUNC_ADD);
                    glBlendFunc(GL_ONE, GL_ONE); //accumulate light
                    
                    //6. Render lights
                    OpenGLSun::getInstance()->Render(sim->views[i]->GetViewTransform()); //Render sun pass
                    
                    for(int h=0; h<sim->lights.size(); h++) //Render light passes
                        sim->lights[h]->Render();
                    
                    //7. Reset OpenGL
                    glUseProgramObjectARB(0);
                    glDisable(GL_STENCIL_TEST);
                    
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE1);
                    glDisable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE2);
                    glDisable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE3);
                    glDisable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    
                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    glDisable(GL_TEXTURE_CUBE_MAP);
                    
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glDisable(GL_TEXTURE_2D);
                    
                    glActiveTexture(GL_TEXTURE6);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glDisable(GL_TEXTURE_2D);
                    
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }
                    /////////// Render reflection
                    {
                    //////////////// Fill reflected G-buffer ////////////////////////
                    sim->views[i]->getGBuffer()->Start(1);
                    sim->views[i]->SetProjection();
                    sim->views[i]->SetReflectedViewTransform(sim->ocean);
                    DrawObjects(sim);
                    sim->views[i]->getGBuffer()->Stop();
                    
                    ///////////////// Render reflected scene with special stencil masking /////////////////////
                    glBindFramebuffer(GL_FRAMEBUFFER, sim->views[i]->getSceneFBO());
                    glDrawBuffer(REFLECTION_ATTACHMENT);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    
                    //1. Create stencil mask
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_STENCIL_TEST);
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    glDisable(GL_BLEND);
                    
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                    glStencilFunc(GL_ALWAYS, 0, 0xFF);
                    glStencilMask(0xFF);
                    glClear(GL_STENCIL_BUFFER_BIT);
                    
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    sim->views[i]->SetProjection();
                    sim->views[i]->GetReflectedViewTransform(sim->ocean).getOpenGLMatrix(openglTrans);
                    glMatrixMode(GL_MODELVIEW);
#ifdef BT_USE_DOUBLE_PRECISION
                    glLoadMatrixd(openglTrans);
#else
                    glLoadMatrixf(openglTrans);
#endif
                    //Render fluid surface and set stencil = 1
                    glDepthMask(GL_FALSE);
                    glDisable(GL_CULL_FACE);
                    glStencilFunc(GL_ALWAYS, 1, 0xFF);
                    sim->ocean->RenderSurface();
                    glDepthMask(GL_TRUE);
                    
                    //Render clipped normal objects and set stencil = 2
                    glStencilFunc(GL_ALWAYS, 2, 0xFF);
                    glClipPlane(GL_CLIP_PLANE0, surface);
                    glEnable(GL_CLIP_PLANE0);
                    DrawObjects(sim);
                    glDisable(GL_CLIP_PLANE0);
                    
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glStencilMask(0x00);
                    
                    //2. Enter deferred rendering
                    glDisable(GL_DEPTH_TEST);
                    
                    OpenGLContent::getInstance()->SetupOrtho();
                    
                    //3. Draw sky where stencil = 1 -> only surface rendered there
                    if(renderSky)
                    {
                        glStencilFunc(GL_EQUAL, 1, 0xFF);
                        OpenGLSky::getInstance()->Render(sim->views[i], sim->views[i]->GetReflectedViewTransform(sim->ocean), sim->zUp);
                    }
                    
                    //4. Bind deferred textures to texture units
                    glStencilFunc(GL_EQUAL, 2, 0xFF); //Draw objects where not only surface was drawn
                    
                    glActiveTexture(GL_TEXTURE0);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getDiffuseTexture()); //Diffuse is reused
                    
                    glActiveTexture(GL_TEXTURE1);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getPositionTexture(1));
                    
                    glActiveTexture(GL_TEXTURE2);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getNormalsTexture(1));
                    
                    glActiveTexture(GL_TEXTURE3);
                    glEnable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getDiffuseCubemap());
                    
                    glActiveTexture(GL_TEXTURE4);
                    glEnable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getReflectionCubemap());
                    
                    //5. Bind empty texture as SAO - no SAO on reflections
                    glActiveTexture(GL_TEXTURE5);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    OpenGLSun::getInstance()->SetTextureUnits(0, 2, 1, 6);
                    OpenGLLight::SetTextureUnits(0, 2, 1, 3, 4, 5, 6);
                    
                    //5. Render ambient pass - sky
                    OpenGLLight::RenderAmbientLight(sim->views[i]->GetReflectedViewTransform(sim->ocean), sim->zUp);
                    
                    glEnable(GL_BLEND);
                    glBlendEquation(GL_FUNC_ADD);
                    glBlendFunc(GL_ONE, GL_ONE); //accumulate light
                    
                    //6. Render lights
                    OpenGLSun::getInstance()->Render(sim->views[i]->GetReflectedViewTransform(sim->ocean)); //Render sun pass
                    
                    for(int h=0; h<sim->lights.size(); h++) //Render light passes
                        sim->lights[h]->Render();
                    
                    //7. Reset OpenGL
                    glUseProgramObjectARB(0);
                    glDisable(GL_STENCIL_TEST);
                    
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE1);
                    glDisable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE2);
                    glDisable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE3);
                    glDisable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    
                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    glDisable(GL_TEXTURE_CUBE_MAP);
                    
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glDisable(GL_TEXTURE_2D);
                    
                    glActiveTexture(GL_TEXTURE6);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glDisable(GL_TEXTURE_2D);
                    
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }
                    /////////// Render refraction
                    {
                    ////////////////// Fill refracted G-buffer //////////////////////////////////////
                    surface[0] = -surface[0];
                    surface[1] = -surface[1];
                    surface[2] = -surface[2];
                    surface[3] = -surface[3];
                    
                    sim->views[i]->getGBuffer()->Start(1);
                    sim->views[i]->SetProjection();
                    sim->views[i]->SetRefractedViewTransform(sim->ocean);
                    DrawObjects(sim);
                    sim->views[i]->getGBuffer()->Stop();
           
                    ///////////////// Render refracted scene with special stencil masking /////////////////////
                    glBindFramebuffer(GL_FRAMEBUFFER, sim->views[i]->getSceneFBO());
                    glDrawBuffer(REFRACTION_ATTACHMENT);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                   
                    //1. Create stencil mask
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_STENCIL_TEST);
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                    glStencilFunc(GL_ALWAYS, 0, 0xFF);
                    glStencilMask(0xFF);
                    glClear(GL_STENCIL_BUFFER_BIT);
                    
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    sim->views[i]->SetProjection();
                    sim->views[i]->GetRefractedViewTransform(sim->ocean).getOpenGLMatrix(openglTrans);
                    glMatrixMode(GL_MODELVIEW);
#ifdef BT_USE_DOUBLE_PRECISION
                    glLoadMatrixd(openglTrans);
#else
                    glLoadMatrixf(openglTrans);
#endif
                    //Render normal objects just to fill depth buffer
                    //glClipPlane(GL_CLIP_PLANE0, surface);
                    //glEnable(GL_CLIP_PLANE0);
                    DrawObjects(sim);
                    //glDisable(GL_CLIP_PLANE0);
                    
                    //Render fluid surface and change stencil to 1
                    glStencilFunc(GL_ALWAYS, 1, 0xFF);
                    sim->ocean->RenderSurface();
                    
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glStencilMask(0x00);
                    
                    //2. Enter deferred rendering
                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    
                    OpenGLContent::getInstance()->SetupOrtho();
                    
                    //3. Bind deferred textures to texture units
                    glStencilFunc(GL_EQUAL, 1, 0xFF);
                    glEnable(GL_BLEND);
                    glBlendEquation(GL_FUNC_ADD);
                    glBlendFunc(GL_ONE, GL_ONE); //accumulate light
                    
                    glActiveTexture(GL_TEXTURE0);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getDiffuseTexture()); //Diffuse is reused
                    
                    glActiveTexture(GL_TEXTURE1);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getPositionTexture(1));
                    
                    glActiveTexture(GL_TEXTURE2);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sim->views[i]->getGBuffer()->getNormalsTexture(1));
                    
                    glActiveTexture(GL_TEXTURE3);
                    glEnable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getDiffuseCubemap());
                    
                    glActiveTexture(GL_TEXTURE4);
                    glEnable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGLSky::getInstance()->getReflectionCubemap());
                    
                    //5. Bind empty texture - no SAO
                    glActiveTexture(GL_TEXTURE5);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    OpenGLSun::getInstance()->SetTextureUnits(0, 2, 1, 6);
                    OpenGLLight::SetTextureUnits(0, 2, 1, 3, 4, 5, 6);
                    
                    //5. Render ambient pass - sky, ssao
                    OpenGLLight::RenderAmbientLight(sim->views[i]->GetRefractedViewTransform(sim->ocean), sim->zUp);
                    
                    //6. Render lights
                    OpenGLSun::getInstance()->Render(sim->views[i]->GetRefractedViewTransform(sim->ocean)); //Render sun pass
                    
                    for(int h=0; h<sim->lights.size(); h++) //Render light passes
                        sim->lights[h]->Render();
                    
                    //7. Reset OpenGL
                    glUseProgramObjectARB(0);
                    glDisable(GL_STENCIL_TEST);
                    
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE1);
                    glDisable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE2);
                    glDisable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    
                    glActiveTexture(GL_TEXTURE3);
                    glDisable(GL_TEXTURE_CUBE_MAP);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    
                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    glDisable(GL_TEXTURE_CUBE_MAP);
                    
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glDisable(GL_TEXTURE_2D);
                    
                    glActiveTexture(GL_TEXTURE6);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glDisable(GL_TEXTURE_2D);
                    
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }
                    
                    ////////// Render water surface
                    {
                    ////////////////// Render water surface using last stencil mask ////////////////
                    glBindFramebuffer(GL_FRAMEBUFFER, sim->views[i]->getSceneFBO());
                    glDrawBuffer(SCENE_ATTACHMENT);
                    glEnable(GL_STENCIL_TEST);
                    glStencilMask(0x00);
                    glStencilFunc(GL_EQUAL, 1, 0xFF);
                    
                    //Render surface without light
                    sim->views[i]->RenderFluidSurface(sim->ocean, false);

                    //Accumulate light
                    glDisable(GL_STENCIL_TEST);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }
                    
                    //////////////// Finish rendering /////////////////////////
                    finalTexture = sim->views[i]->getSceneTexture();
                }
            /////////////////////////////////////////////////////////////////////////////////////
            }
*/