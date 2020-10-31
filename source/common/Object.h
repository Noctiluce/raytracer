#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "common.h"
#include "BoundingVolumeHierarchyNode.h"

#define EPSILON  1e-3


class Object{
public:

    std::string name;

    friend class Sphere;
    friend class Square;
    friend class Model;

    typedef struct{
        vec4 color;
        std::string texture_path;
        float Kd;
        float Ks;
        float Kn;
        float Kt;
        float Ka;
        float Kr;
    } ShadingValues;

    typedef struct{
        double t;
        vec4 P;
        vec4 N;
        int ID_;
        std::string name;
    } IntersectionValues;
    
    bool isBubble;

    Object(std::string name): name(name)  {};
    ~Object() {};

    Mesh mesh;
    ShadingValues shadingValues;

private:
    mat4 C;
    mat4 INVC;
    mat4 INVCStar;
    mat4 TRANINVC;

public:

    void setShadingValues(ShadingValues _shadingValues){shadingValues = _shadingValues;}

    void setModelView(mat4 modelview){
        C = modelview;
        INVC = invert(modelview);
        mat4 CStar = modelview;
        CStar[0][3] = 0;
        CStar[1][3] = 0;
        CStar[2][3] = 0;
        INVCStar = invert(CStar);
        TRANINVC = transpose(invert(modelview));
    }

    mat4 getModelView(){ return C; }

    virtual IntersectionValues intersect(vec4 p0, vec4 V)=0;


};

class Sphere : public Object{
public:
    
    Sphere(std::string name, vec3 center= vec3(0., 0., 0.), double radius=1.) : Object(name), center(center), radius(radius) { mesh.makeSubdivisionSphere(8, center, radius); };
    
    virtual IntersectionValues intersect(vec4 p0, vec4 V);
    
private:
    double raySphereIntersection(vec4 p0, vec4 V);
    vec3 center;
    double radius;
};


class Square : public Object{
public:

    Square(std::string name, mat4 transform = mat4(1.0f)) : Object(name) {

        mesh.vertices.resize(6);
        mesh.uvs.resize(6);
        mesh.normals.resize(6);

        mesh.vertices[0]=transform*vec4(-1.0, -1.0, 0.0, 1.0);
        mesh.uvs[0] = vec2(0.0,0.0);
        mesh.vertices[1]=transform*vec4(1.0, 1.0, 0.0, 1.0);
        mesh.uvs[1] = vec2(1.0,1.0);
        mesh.vertices[2]=transform*vec4(1.0, -1.0, 0.0, 1.0);
        mesh.uvs[2] = vec2(1.0,0.0);

        mesh.vertices[3]=transform*vec4(-1.0, -1.0, 0.0, 1.0);
        mesh.uvs[3] = vec2(0.0,0.0);
        mesh.vertices[4]=transform*vec4(1.0, 1.0, 0.0, 1.0);
        mesh.uvs[4] = vec2(1.0,1.0);
        mesh.vertices[5]=transform*vec4(-1.0, 1.0, 0.0, 1.0);
        mesh.uvs[5] = vec2(0.0,1.0);

        point = mesh.vertices[0];
        TRANINVC = transpose(invert(transform));
        vec4 N (0, 0, 1.0, 0.);
        N = TRANINVC*N;
        normal = vec3(N.x, N.y, N.z);
        for( unsigned i = 0 ; i < 6 ; i++){
            mesh.normals[i]= normal;
        }

    };

    virtual IntersectionValues intersect(vec4 p0, vec4 V);

private:
    double raySquareIntersection(vec4 p0, vec4 V);
    vec4 point;
    vec3 normal;
};

class Model : public Object{
public:
    
    Model(std::string name, mat4 transform = mat4(1.0f)) : Object(name) 
    {
        mesh.loadOBJ(name.c_str()); 

        std::vector<Triangle*> tris;
        root = new BoundingVolumeHierarchyNode();
        
        for(int i = 0; i < mesh.vertices.size(); ++i) {
           mesh.vertices[i] = transform * mesh.vertices[i];
           
           if (i % 3 == 0 && i != 0)
           {
               Triangle * tmp = new Triangle();
               tmp->v0 = vec4tovec3(mesh.vertices[i - 3]);
               tmp->v1 = vec4tovec3(mesh.vertices[i - 2]);
               tmp->v2 = vec4tovec3(mesh.vertices[i - 1]);
               tmp->n0 = mesh.normals[i - 3];
               tmp->n1 = mesh.normals[i - 2];
               tmp->n2 = mesh.normals[i - 1];
               tris.push_back(tmp);
           }
        }
        
        root = root->build(tris);
        
        mesh.box_min = vec4tovec3(transform * vec3tovec4(mesh.box_min));
        mesh.box_max = vec4tovec3(transform * vec3tovec4(mesh.box_max));
        mesh.center = vec4tovec3(transform * vec3tovec4(mesh.center));
    }
    
    virtual IntersectionValues intersect(vec4 p0, vec4 V);
    
private:
    bool rayTriangleIntersection(vec3 orig, vec3 dir, const vec3 & v0, const vec3 & v1, const vec3 & v2, double & u, double & v, double & t);
    bool rayBoxIntersection(vec3 ray_orig, vec3 ray_dir, double & t, vec3 box_min, vec3 box_max);
    
    bool hit(BoundingVolumeHierarchyNode * node, vec3 ray_orig, vec3 ray_dir, double & t, double & tmin, vec3 & normal);

    
    vec3 vec4tovec3(vec4 v){return vec3(v.x, v.y, v.z);}
    vec4 vec3tovec4(vec3 v){return vec4(v.x, v.y, v.z, 1.0);}
    
    BoundingVolumeHierarchyNode * root;
};

#endif /* defined(__OBJECT_H__) */
