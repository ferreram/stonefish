//
//  Compound.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 19/09/17.
//  Copyright (c) 2017-2018 Patryk Cieslak. All rights reserved.
//

#include "entities/solids/Compound.h"

#include "core/SimulationApp.h"
#include "core/SimulationManager.h"

namespace sf
{

Compound::Compound(std::string uniqueName, SolidEntity* firstExternalPart, const Transform& origin, bool enableHydrodynamicForces)
    : SolidEntity(uniqueName, Material(), 0, Scalar(-1), enableHydrodynamicForces)
{
    //All transformations are zero -> transforming the origin of a compound body doesn't make sense...
    phyMesh = NULL; // There is no single mesh
    volume = 0;
	mass = 0;
	Ipri = Vector3(0,0,0);
    
	AddExternalPart(firstExternalPart, origin);
}

Compound::~Compound()
{
    for(unsigned int i=0; i<parts.size(); ++i)
        delete parts[i].solid;
    parts.clear();
}
    
Material Compound::getMaterial(size_t partId) const
{
    if(partId < parts.size())
        return parts[partId].solid->getMaterial();
    else
        return Material();
}

size_t Compound::getPartId(size_t collisionShapeId) const
{
    if(collisionShapeId < collisionPartId.size())
        return collisionPartId[collisionShapeId];
    else
        return 0;
}
    
SolidType Compound::getSolidType()
{
    return SolidType::SOLID_COMPOUND;
}

std::vector<Vertex>* Compound::getMeshVertices()
{
    std::vector<Vertex>* pVert = new std::vector<Vertex>(0);
        
    for(size_t i=0; i<parts.size(); ++i)
    {
        if(parts[i].isExternal)
        {
            std::vector<Vertex>* pPartVert = parts[i].solid->getMeshVertices();
            Transform phyMeshTrans = (parts[i].origin
                                     * parts[i].solid->getCG2OTransform().inverse()
                                     * parts[i].solid->getCG2CTransform());
            
            glm::mat4 glTrans = glMatrixFromTransform(phyMeshTrans);
            
            for(size_t h=0; h < pPartVert->size(); ++h)
            {
                glm::vec4 vTrans = glTrans * glm::vec4((*pPartVert)[h].pos, 1.f);
                Vertex v;
                v.pos = glm::vec3(vTrans);
                pVert->push_back(v);
            }
                
            delete pPartVert;
        }
    }
        
    return pVert;
}
    
void Compound::AddInternalPart(SolidEntity* solid, const Transform& origin)
{
    if(solid != NULL)
    {
        CompoundPart part;
        part.solid = solid;
        part.origin = origin;
        part.isExternal = false;
        parts.push_back(part);
		RecalculatePhysicalProperties();
    }
}

void Compound::AddExternalPart(SolidEntity* solid, const Transform& origin)
{
    if(solid != NULL)
    {
        CompoundPart part;
        part.solid = solid;
        part.origin = origin;
        part.isExternal = true;
        parts.push_back(part);
		RecalculatePhysicalProperties();
    }
}

void Compound::RecalculatePhysicalProperties()
{
	//Calculate rigid body properties
    /*
      1. Calculate compound mass and compound CG (sum of location - local * m / M)
      2. Calculate inertia of part in global frame and compound CG (rotate and translate inertia tensor) 3x3
         and calculate compound inertia 3x3 (sum of parts inertia)
      3. Calculate primary moments of inertia
      4. Find primary axes of inertia
      5. Rotate frame to match primary axes and move to CG
    */
     
    //1. Calculate compound mass, CG and CB
    Vector3 compoundCG(0,0,0); //In compound body origin frame
    Vector3 compoundCB(0,0,0); //In compound body origin frame
    Scalar compoundMass = 0;
	Scalar compoundVolume = 0;
    T_CG2O = T_CG2C = T_CG2G = Transform::getIdentity();
    P_CB = Vector3(0,0,0);
        
    for(size_t i=0; i<parts.size(); ++i)
    {
        compoundMass += parts[i].solid->getMass();
        compoundCG += (parts[i].origin * parts[i].solid->getCG2OTransform().inverse()).getOrigin() * parts[i].solid->getMass();
        
        if(parts[i].solid->isBuoyant())
        {
            compoundVolume += parts[i].solid->getVolume();
            compoundCB += parts[i].origin * parts[i].solid->getCG2OTransform().inverse() * parts[i].solid->getCB() * parts[i].solid->getVolume();
        }
    }
    
    //Set transform origin
    compoundCG /= compoundMass;
    if(compoundVolume > Scalar(0)) compoundCB /= compoundVolume;
    T_CG2O.setOrigin(-compoundCG);
    
    //2. Calculate compound inertia matrix
    Matrix3 I = Matrix3(0,0,0,0,0,0,0,0,0);
        
    for(unsigned int i=0; i<parts.size(); ++i)
    {
        //Calculate inertia matrix 3x3 of solid in the compound body origin frame and transform to CB
        Vector3 solidPriInertia = parts[i].solid->getInertia();
        Matrix3 solidInertia = Matrix3(solidPriInertia.x(), 0, 0, 0, solidPriInertia.y(), 0, 0, 0, solidPriInertia.z());
            
        //Rotate inertia tensor from part CG to compound CG
        Transform compToPart = T_CG2O * parts[i].origin * parts[i].solid->getCG2OTransform().inverse();
        solidInertia = compToPart.getBasis() * solidInertia * compToPart.getBasis().transpose();
            
        //Translate inertia tensor from part CG to compound CG
        Vector3 t = compToPart.getOrigin();
        Scalar m = parts[i].solid->getMass();
        solidInertia += Matrix3(t.y()*t.y()+t.z()*t.z(),            -t.x()*t.y(),            -t.x()*t.z(),
                                           -t.y()*t.x(), t.x()*t.x()+t.z()*t.z(),            -t.y()*t.z(),
                                           -t.z()*t.x(),            -t.z()*t.y(), t.x()*t.x()+t.y()*t.y()).scaled(Vector3(m, m, m));
            
        //Accumulate inertia tensor
        I += solidInertia;
    }
	
	//3. Find compound moments of inertia
	Vector3 compoundPriInertia(I.getRow(0).getX(), I.getRow(1).getY(), I.getRow(2).getZ());
	
	//Check if inertia matrix is not diagonal
	if(!(btFuzzyZero(I.getRow(0).getY()) && btFuzzyZero(I.getRow(0).getZ())
	     && btFuzzyZero(I.getRow(1).getX()) && btFuzzyZero(I.getRow(1).getZ())
	     && btFuzzyZero(I.getRow(2).getX()) && btFuzzyZero(I.getRow(2).getY())))
	{
		//3.1. Calculate principal moments of inertia
		Scalar T = I[0][0] + I[1][1] + I[2][2]; //Ixx + Iyy + Izz
		Scalar II = I[0][0]*I[1][1] + I[0][0]*I[2][2] + I[1][1]*I[2][2] - I[0][1]*I[0][1] - I[0][2]*I[0][2] - I[1][2]*I[1][2]; //Ixx Iyy + Ixx Izz + Iyy Izz - Ixy^2 - Ixz^2 - Iyz^2
		Scalar U = btSqrt(T*T-Scalar(3.)*II)/Scalar(3.);
		Scalar theta = btAcos((-Scalar(2.)*T*T*T + Scalar(9.)*T*II - Scalar(27.)*I.determinant())/(Scalar(54.)*U*U*U));
		Scalar A = T/Scalar(3.) - Scalar(2.)*U*btCos(theta/Scalar(3.));
		Scalar B = T/Scalar(3.) - Scalar(2.)*U*btCos(theta/Scalar(3.) - Scalar(2.)*M_PI/Scalar(3.));
		Scalar C = T/Scalar(3.) - Scalar(2.)*U*btCos(theta/Scalar(3.) + Scalar(2.)*M_PI/Scalar(3.));
		compoundPriInertia = Vector3(A, B, C);
    
		//3.2. Calculate principal axes of inertia
		Matrix3 L;
		Vector3 axis1,axis2,axis3;
		axis1 = findInertiaAxis(I, A);
		axis2 = findInertiaAxis(I, B);
		axis3 = axis1.cross(axis2);
		axis2 = axis3.cross(axis1);
    
		//3.3. Rotate body so that principal axes are parallel to (x,y,z) system
		Matrix3 rotMat(axis1[0],axis2[0],axis3[0], axis1[1],axis2[1],axis3[1], axis1[2],axis2[2],axis3[2]);
		T_CG2O = Transform(rotMat, Vector3(0,0,0)).inverse() * T_CG2O;
    }
	
    T_CG2C = T_CG2G = T_CG2O;
    
    //Move CB to compound CG frame
    P_CB = T_CG2O * compoundCB;
    
	mass = compoundMass;
	volume = compoundVolume;
	Ipri = compoundPriInertia;
    
    ComputeHydrodynamicProxy(HYDRO_PROXY_ELLIPSOID);
}

btCollisionShape* Compound::BuildCollisionShape()
{
	//Build collision shape from external parts
    btCompoundShape* colShape = new btCompoundShape();
    for(size_t i = 0; i<parts.size(); ++i)
    {
        if(parts[i].isExternal)
        {
            Transform childTrans = parts[i].origin * parts[i].solid->getCG2OTransform().inverse() * parts[i].solid->getCG2CTransform();
            btCollisionShape* partColShape = parts[i].solid->BuildCollisionShape();
            colShape->addChildShape(childTrans, partColShape);
            collisionPartId.push_back(i);
        }
    }
	return colShape;
}

void Compound::ComputeFluidForces(HydrodynamicsSettings settings, Ocean* liquid)
{
    if(!computeHydro) return;
    
    BodyFluidPosition bf = CheckBodyFluidPosition(liquid);
    
    //If completely outside fluid just set all torques and forces to 0
    if(bf == BodyFluidPosition::OUTSIDE_FLUID)
    {
        Fb.setZero();
        Tb.setZero();
        Fds.setZero();
        Tds.setZero();
        Fdp.setZero();
        Tdp.setZero();
        return;
    }
    
    if(bf == BodyFluidPosition::INSIDE_FLUID)
    {
        //Compute buoyancy based on CB position
        if(isBuoyant())
        {
            Fb = -volume*liquid->getLiquid()->density * SimulationApp::getApp()->getSimulationManager()->getGravity();
            Tb = (getCGTransform() * P_CB - getCGTransform().getOrigin()).cross(Fb);
        }
        
        if(settings.dampingForces)
        {
            //Set zero
            Fds.setZero();
            Tds.setZero();
            Fdp.setZero();
            Tdp.setZero();
            
            //Get velocity data
            Vector3 v = getLinearVelocity();
            Vector3 omega = getAngularVelocity();
            
            //Create temporary vectors for summing
            Vector3 Fdsp(0,0,0);
            Vector3 Tdsp(0,0,0);
            Vector3 Fdpp(0,0,0);
            Vector3 Tdpp(0,0,0);
            
            for(size_t i=0; i<parts.size(); ++i) //Go through all parts
                if(parts[i].isExternal) //Compute drag only for external parts
                {
                    Transform T_C_part = getOTransform() * parts[i].origin * parts[i].solid->getO2CTransform();
                    ComputeFluidForcesSubmerged(parts[i].solid->getPhysicsMesh(), liquid, getCGTransform(), T_C_part, v, omega, Fdsp, Tdsp, Fdpp, Tdpp);
                    Fds += Fdsp;
                    Tds += Tdsp;
                    Fdp += Fdpp;
                    Tdp += Tdpp;
                }
        }
    }
    else //CROSSING FLUID SURFACE
    {
        if(settings.reallisticBuoyancy || settings.dampingForces)
        {
            //Clear forces that will be recomputed
            if(settings.reallisticBuoyancy)
            {
                Fb.setZero();
                Tb.setZero();
            }
        
            if(settings.dampingForces)
            {
                Fds.setZero();
                Tds.setZero();
                Fdp.setZero();
                Tdp.setZero();
            }
        
            //Get velocity data
            Vector3 v = getLinearVelocity();
            Vector3 omega = getAngularVelocity();
        
            //Create temporary vectors for summing
            Vector3 Fbp(0,0,0);
            Vector3 Tbp(0,0,0);
            Vector3 Fdsp(0,0,0);
            Vector3 Tdsp(0,0,0);
            Vector3 Fdpp(0,0,0);
            Vector3 Tdpp(0,0,0);
	
            for(size_t i=0; i<parts.size(); ++i) //Loop through all parts
            {
                Transform T_C_part = getOTransform() * parts[i].origin * parts[i].solid->getO2CTransform();
                HydrodynamicsSettings pSettings = settings;
                pSettings.reallisticBuoyancy &= parts[i].solid->isBuoyant();
                
                if(parts[i].isExternal) //Compute buoyancy and drag
                {
                    ComputeFluidForcesSurface(pSettings, parts[i].solid->getPhysicsMesh(), liquid, getCGTransform(), T_C_part, v, omega, Fbp, Tbp, Fdsp, Tdsp, Fdpp, Tdpp);
                    Fb += Fbp;
                    Tb += Tbp;
                    Fds += Fdsp;
                    Tds += Tdsp;
                    Fdp += Fdpp;
                    Tdp += Tdpp;
                }
                else if(pSettings.reallisticBuoyancy) //Compute only buoyancy
                {
                    pSettings.dampingForces = false;
                    ComputeFluidForcesSurface(pSettings, parts[i].solid->getPhysicsMesh(), liquid, getCGTransform(), T_C_part, v, omega, Fbp, Tbp, Fdsp, Tdsp, Fdpp, Tdpp);
                    Fb += Fbp;
                    Tb += Tbp;
                }
            }
        }
    }
        
    if(settings.dampingForces)
        CorrectDampingForces();
}

void Compound::BuildGraphicalObject()
{
	for(unsigned int i=0; i<parts.size(); ++i)
		parts[i].solid->BuildGraphicalObject();
}

std::vector<Renderable> Compound::Render()
{
	std::vector<Renderable> items(0);
	
	if(isRenderable())
	{
        Renderable item;
        item.type = RenderableType::SOLID_CS;
        item.model = glMatrixFromTransform(getCGTransform());
        items.push_back(item);
        
        Vector3 cbWorld = getCGTransform() * P_CB;
        item.type = RenderableType::HYDRO_CS;
        item.model = glMatrixFromTransform(Transform(Quaternion::getIdentity(), cbWorld));
        item.points.push_back(glm::vec3(volume, volume, volume));
        items.push_back(item);
        item.points.clear();
        
        item.type = RenderableType::HYDRO_ELLIPSOID;
        item.model = glMatrixFromTransform(getHTransform());
        item.points.push_back(glm::vec3((GLfloat)hydroProxyParams[0], (GLfloat)hydroProxyParams[1], (GLfloat)hydroProxyParams[2]));
        items.push_back(item);
        item.points.clear();
        
        Transform oCompoundTrans = getOTransform();
        item.type = RenderableType::SOLID;
        
		for(unsigned int i=0; i<parts.size(); ++i)
		{
			Transform oTrans = oCompoundTrans * parts[i].origin * parts[i].solid->getO2GTransform();
			item.objectId = parts[i].solid->getObject();
			item.lookId = parts[i].solid->getLook();
			item.model = glMatrixFromTransform(oTrans);
			items.push_back(item);
		}
        
        //Forces
        Vector3 cg = getCGTransform().getOrigin();
        glm::vec3 cgv((GLfloat)cg.x(), (GLfloat)cg.y(), (GLfloat)cg.z());
        item.points.clear();
        item.points.push_back(cgv);
        item.model = glm::mat4(1.f);
        
        item.type = RenderableType::FORCE_BUOYANCY;
        item.points.push_back(cgv + glm::vec3((GLfloat)Fb.x(), (GLfloat)Fb.y(), (GLfloat)Fb.z())/1000.f);
        items.push_back(item);
        
        item.points.pop_back();
        item.type = RenderableType::FORCE_LINEAR_DRAG;
        item.points.push_back(cgv + glm::vec3((GLfloat)Fds.x(), (GLfloat)Fds.y(), (GLfloat)Fds.z()));
        items.push_back(item);
        
        item.points.pop_back();
        item.type = RenderableType::FORCE_QUADRATIC_DRAG;
        item.points.push_back(cgv + glm::vec3((GLfloat)Fdp.x(), (GLfloat)Fdp.y(), (GLfloat)Fdp.z()));
        items.push_back(item);
	}
		
	return items;
}

}