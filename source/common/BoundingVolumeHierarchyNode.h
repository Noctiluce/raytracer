// Inspire du travail de :
// https://blog.frogslayer.com/kd-trees-for-faster-ray-tracing-with-triangles/

#ifndef BOUNDINGVOLUMEHIERARCHYNODE_H
#define BOUNDINGVOLUMEHIERARCHYNODE_H

#include "common.h"

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2

struct BoundingBox
{
    double min_x;
    double max_x;
    double min_y;
    double max_y;
    double min_z;
    double max_z;
    int longest_axis;
    
    // on etand le bounding volume avec une autre box
    void expand(BoundingBox in_box)
    {
        min_x = std::min(min_x, in_box.min_x);
        max_x = std::max(max_x, in_box.max_x);
        
        min_y = std::min(min_y, in_box.min_y);
        max_y = std::max(max_y, in_box.max_y);
        
        min_z = std::min(min_z, in_box.min_z);
        max_z = std::max(max_z, in_box.max_z);
        
        if (max_x >= max_y && max_x >= max_z) longest_axis = AXIS_X;
        if (max_y >= max_x && max_y >= max_z) longest_axis = AXIS_Y;
        if (max_z >= max_y && max_z >= max_x) longest_axis = AXIS_Z;
    }
};

struct Triangle
{
    vec3 v0, v1, v2;
    vec3 n0, n1, n2;
    
    // on recupere la boudingbox d'un triangle
    BoundingBox get_bounding_box()
    {
        BoundingBox b = BoundingBox();
        b.min_x = std::min({v0.x, v1.x, v2.x});
        b.max_x = std::max({v0.x, v1.x, v2.x});
        
        b.min_y = std::min({v0.y, v1.y, v2.y});
        b.max_y = std::max({v0.y, v1.y, v2.y});
        
        b.min_z = std::min({v0.z, v1.z, v2.z});
        b.max_z = std::max({v0.z, v1.z, v2.z});
        
        if (b.max_x >= b.max_y && b.max_x >= b.max_z) b.longest_axis = AXIS_X;
        if (b.max_y >= b.max_x && b.max_y >= b.max_z) b.longest_axis = AXIS_Y;
        if (b.max_z >= b.max_y && b.max_z >= b.max_x) b.longest_axis = AXIS_Z;

        return b;
    }
    
    // on recupere le barycentre du triangle
    vec3 get_midpoint()
    {
        return (v0 + v1 + v2) / 3.0;
    }
};

// class d'hierarchie de volumes englobants
// *****
class BoundingVolumeHierarchyNode {
public :
    BoundingBox bbox;
    std::vector<Triangle *> triangles;

    BoundingVolumeHierarchyNode * left;
    BoundingVolumeHierarchyNode * right;
    
    BoundingVolumeHierarchyNode(){}
    
    // on construit l'arbre binaire
    BoundingVolumeHierarchyNode * build(std::vector<Triangle*> & tris, int depth = 0) const
    {
        BoundingVolumeHierarchyNode * node = new BoundingVolumeHierarchyNode();
        node->triangles = tris;
        node->left = NULL;
        node->right = NULL;
        node->bbox = BoundingBox();
        
        if (tris.size() == 0) return node;
        
        if (tris.size() == 1) 
        {
            node->bbox = tris[0]->get_bounding_box();
            node->left = new BoundingVolumeHierarchyNode();
            node->right = new BoundingVolumeHierarchyNode();
            node->left->triangles = std::vector<Triangle*>();
            node->right->triangles = std::vector<Triangle*>();
            return node;
        }
        
        // bounding box qui englobe tous les triangles
        node->bbox = tris[0]->get_bounding_box();
        for(int i = 1 ; i < tris.size(); ++i)
            node->bbox.expand(tris[i]->get_bounding_box());
        
        // milieu de la boite englobante
        vec3 midpt = vec3(0);
        for(int i = 0; i < tris.size(); ++i)
            midpt = midpt + (tris[i]->get_midpoint() * (1.0 / tris.size()));
        
        std::vector<Triangle*> left_tris;
        std::vector<Triangle*> right_tris;
        int axis = node->bbox.longest_axis;
        
        // on "coupe" en deux en fonction de l'axe le plus long
        for(int i = 0 ; i < tris.size(); ++i)
        {
            switch (axis)
            {
                case AXIS_X :
                    midpt.x >= tris[i]->get_midpoint().x ? right_tris.push_back(tris[i]) : left_tris.push_back(tris[i]);
                    break;
                    
                case AXIS_Y :
                    midpt.y >= tris[i]->get_midpoint().y ? right_tris.push_back(tris[i]) : left_tris.push_back(tris[i]);
                    break;
                    
                case AXIS_Z :
                    midpt.z >= tris[i]->get_midpoint().z ? right_tris.push_back(tris[i]) : left_tris.push_back(tris[i]);
                    break;
            }
        }
        if (left_tris.size() == 0 && right_tris.size() > 0) left_tris = right_tris;
        if (right_tris.size() == 0 && left_tris.size() > 0) right_tris = left_tris;
        
        int matches = 0;
        for (int i = 0; i < left_tris.size(); ++i)
        {
            for (int j = 0; j < right_tris.size(); ++j)
            {
                if (left_tris[i] == right_tris[j]) matches++;
            }
        }
        // 25 % des triangles matchent, on arrete de diviser
        if ((float) matches / left_tris.size() < 0.25 && (float) matches / right_tris.size() < 0.25) 
        {
            node->left = build(left_tris, depth + 1);
            node->right = build(right_tris, depth + 1);
        }
        else
        {
            node->left = new BoundingVolumeHierarchyNode();
            node->right = new BoundingVolumeHierarchyNode();
            node->left->triangles = std::vector<Triangle*>();
            node->right->triangles = std::vector<Triangle*>();
        }
        return node;
    }
};
#endif
