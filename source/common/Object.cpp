#include "common.h"


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
Object::IntersectionValues Sphere::intersect(vec4 p0, vec4 V) {
	IntersectionValues result;
	result.t = raySphereIntersection(p0, V);
	if (nearlyEqual(result.t, 0.0, EPSILON)) result.t = 0.0f;
	if (nearlyEqual(result.t, 0.0, -EPSILON)) result.t = 0.0f;

	result.P = p0 + V * result.t;

	result.N = normalize(result.P - center);
	if (nearlyEqual(result.N.x, 0.0, EPSILON)) result.N.x = 0.0f;
	if (nearlyEqual(result.N.y, 0.0, EPSILON)) result.N.y = 0.0f;
	if (nearlyEqual(result.N.z, 0.0, EPSILON)) result.N.z = 0.0f;
	if (nearlyEqual(result.N.x, 0.0, -EPSILON)) result.N.x = 0.0f;
	if (nearlyEqual(result.N.y, 0.0, -EPSILON)) result.N.y = 0.0f;
	if (nearlyEqual(result.N.z, 0.0, -EPSILON)) result.N.z = 0.0f;
	result.name = name;
	return result;
}

/* -------------------------------------------------------------------------- */
/* ------ Ray = p0 + t*V  sphere at origin center and radius radius    : Find t ------- */
double Sphere::raySphereIntersection(vec4 p0, vec4 V) {
	double t = std::numeric_limits< double >::infinity();

	// discrimant :  D = b^2-4ac pour ax^2 + bx +c = 0
	// D > 0 alors x1 = (-b +sqrt(D))/(2a) et x2 = (-b-sqrt(D))/(2a)
	// D = 0 alors x = -b/(2a)
	// D < 0 alors pas de solutions

	// t^2 V*V + 2t *V(po-center) + norm(po-center)^2 - radius^2 =0
	// ax^2 + bx +c = 0
	double a = dot(V, V);
	double b = 2.0*dot(V, p0 - center);
	double c = dot(p0 - center, p0 - center) - std::pow(radius, 2);
    
        
	double discrimant = std::pow(b, 2) - 4.0*a*c;
	if (discrimant > 0) // deux solutions
	{
		double sqrtD = sqrt(discrimant);
		double t1 = (-b + sqrtD) / (2.0*a);
		double t2 = (-b - sqrtD) / (2.0*a);
        
        //if(t1 <0 && t2 <0) t = 0.01;//std::cout<<"heyllo"<<std::endl;
        
		if (t1 > 0 && t1 < t2) t = t1;
		else if (t2 > 0 && t2 < t1) t = t2;
	}
	else if (nearlyEqual(discrimant, 0.0, EPSILON)) // une solution
		t = -b / (2.0*a);

	// retourne la distance de l'intersection depuis l'origine p0
    //if(t < 20)std::cout << "t = " << t << std::endl;
	return t;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
Object::IntersectionValues Square::intersect(vec4 p0, vec4 V) {
	IntersectionValues result;
	result.t = raySquareIntersection(p0, V);
	if (nearlyEqual(result.t, 0.0, EPSILON)) result.t = 0.0f;
	if (nearlyEqual(result.t, 0.0, -EPSILON)) result.t = 0.0f;

	//vec3 v0 = vec3(mesh.vertices[0].x, mesh.vertices[0].y, mesh.vertices[0].z); // bas gauche
	//vec3 v1 = vec3(mesh.vertices[2].x, mesh.vertices[2].y, mesh.vertices[2].z); // bas droit
	//vec3 v2 = vec3(mesh.vertices[1].x, mesh.vertices[1].y, mesh.vertices[1].z); // haut droit
	//result.N = normalize(cross(v1 - v0, v2 - v0));
    
    result.N = normal; // == normalize((1.0 - mesh.uvs[0].x - mesh.uvs[0].y) * mesh.normals[0] + mesh.uvs[1].x * mesh.normals[1] + mesh.uvs[2].y * mesh.normals[2]);
	if (nearlyEqual(result.N.x, 0.0, EPSILON)) result.N.x = 0.0f;
	if (nearlyEqual(result.N.y, 0.0, EPSILON)) result.N.y = 0.0f;
	if (nearlyEqual(result.N.z, 0.0, EPSILON)) result.N.z = 0.0f;
	if (nearlyEqual(result.N.x, 0.0, -EPSILON)) result.N.x = 0.0f;
	if (nearlyEqual(result.N.y, 0.0, -EPSILON)) result.N.y = 0.0f;
	if (nearlyEqual(result.N.z, 0.0, -EPSILON)) result.N.z = 0.0f;

	result.name = name;
	result.P = p0 + V * result.t;

	return result;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
double Square::raySquareIntersection(vec4 p0, vec4 V) {
	
	double t = std::numeric_limits< double >::infinity();

	vec3 o = vec3(p0.x, p0.y, p0.z);
	vec3 d = normalize(vec3(V.x, V.y, V.z));
	
	vec3 v0 = vec3(mesh.vertices[0].x, mesh.vertices[0].y, mesh.vertices[0].z); // bas gauche
	vec3 v1 = vec3(mesh.vertices[2].x, mesh.vertices[2].y, mesh.vertices[2].z); // bas droit
	vec3 v2 = vec3(mesh.vertices[1].x, mesh.vertices[1].y, mesh.vertices[1].z); // haut droit
	vec3 v3 = vec3(mesh.vertices[5].x, mesh.vertices[5].y, mesh.vertices[5].z); // haut gauche

	vec3 n = normalize(cross(v1 - v0, v2 - v0));
	vec3 a = vec3(point.x, point.y, point.z);

	double dn = dot(d, n);
    if(dn != 0) // sinon division par zero ! Aie
	{
		double tmp = (dot(a -o, n)) / dn;
		if (tmp >= EPSILON) 
		// si inf alors derriere camera
		// si sup alors devant camera
		// si nul alors confondu avec le plan
		// si non défini alors rayon parallele et distinct
		{
			vec3 onSquare = o + d * tmp;

			vec3 n0 = normalize(cross(onSquare - v0, v1 - v0));
			vec3 n1 = normalize(cross(onSquare - v1, v2 - v1));
			vec3 n2 = normalize(cross(onSquare - v2, v3 - v2));
			vec3 n3 = normalize(cross(onSquare - v3, v0 - v3));
			
			double s0 = dot(n0, n);
			double s1 = dot(n1, n);
			double s2 = dot(n2, n);
			double s3 = dot(n3, n);
	
			if ((s0 > 0 && s1 >0 && s2 >0 && s3 >0)||(s0 <0 && s1 <0 && s2 <0 && s3 <0))
				t = tmp;

			// test appartenance au bord du carre			
			vec3 c1 = v1 - v0; // cote du bas
			vec3 c2 = v3 - v0; // cote gauche
			vec3 c3 = v1 - v2; // cote droite
			vec3 c4 = v3 - v2; // cote du haut

			if((dot(c1, onSquare-v0)==0 && length(onSquare-v0) <= length(c1))||(dot(c2, onSquare-v0)==0 && length(onSquare-v0) <= length(c2))
				|| (dot(c3, onSquare-v2)==0 && length(onSquare-v2) <= length(c3)) ||  (dot(c4, onSquare-v2)==0 && length(onSquare-v2) <= length(c4)))
				t = tmp;
		}
	} 
	return t;
	
}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

bool Model::hit(BoundingVolumeHierarchyNode * node, vec3 ray_orig, vec3 ray_dir, double & t, double & tmin, vec3 & normal) {
    
    vec3 min_box = vec3(node->bbox.min_x, node->bbox.min_y, node->bbox.min_z);
    vec3 max_box = vec3(node->bbox.max_x, node->bbox.max_y, node->bbox.max_z);
    
    // on test s'il y a une intersection entre le rayon et la boite
    if (rayBoxIntersection(ray_orig, ray_dir, t, min_box, max_box)) {    
        bool hit_tri = false;
        int bestIndice;
        double u, v; 
        // le noeud courant n'est pas une feuille, alors on parcourt recursivement son fils droit et gauche
        if (node->left->triangles.size() > 0 || node->right->triangles.size() > 0) {
            bool hitleft = hit(node->left, ray_orig, ray_dir, t, tmin, normal); 
            bool hitright = hit(node->right, ray_orig, ray_dir, t, tmin, normal);
            return hitleft || hitright;
        } 
        else // le noeud courant est une feuille
        {
            for(int i = 0; i < node->triangles.size(); ++i) {
                
                // on test qu'il y a une intersection du triangle et si ce triangle est le plus proche de la camera
                if (rayTriangleIntersection(ray_orig, ray_dir, node->triangles[i]->v0, node->triangles[i]->v1, node->triangles[i]->v2, u, v, t) && t < tmin) {
                    hit_tri = true;
                    bestIndice = i; 
                    tmin = t;
                }
            }
            
            if (hit_tri)
            {
                // on actualise t
                t = tmin;
                // on applique le lissage pour les normales
                normal = normalize((1.0 - u - v) * node->triangles[bestIndice]->n0 + u * node->triangles[bestIndice]->n1 + v * node->triangles[bestIndice]->n2);
                return true; // triangle touche
            }
        }
    }
    
    return false;
}




Object::IntersectionValues Model::intersect(vec4 p0, vec4 V) {
	IntersectionValues result;
	double tmin = +std::numeric_limits< double >::infinity();
    double t = tmin;
    vec3 normal = vec3(0);
    
    if(hit(root, vec4tovec3(p0), vec4tovec3(V), t, tmin, normal))
    {
        result.t = t;
        if (nearlyEqual(result.t, 0.0, EPSILON)) result.t = 0.0f;
        if (nearlyEqual(result.t, 0.0, -EPSILON)) result.t = 0.0f;

        result.N = normal;
        if (nearlyEqual(result.N.x, 0.0, EPSILON)) result.N.x = 0.0f;
        if (nearlyEqual(result.N.y, 0.0, EPSILON)) result.N.y = 0.0f;
        if (nearlyEqual(result.N.z, 0.0, EPSILON)) result.N.z = 0.0f;
        if (nearlyEqual(result.N.x, 0.0, -EPSILON)) result.N.x = 0.0f;
        if (nearlyEqual(result.N.y, 0.0, -EPSILON)) result.N.y = 0.0f;
        if (nearlyEqual(result.N.z, 0.0, -EPSILON)) result.N.z = 0.0f;
        
        result.P = p0 + V * result.t;
        result.name = name;
    }
    else
        result.t = std::numeric_limits< double >::infinity();
    
	return result;
}




/* -------------------------------------------------------------------------- */
// Algorithme d'intersection de Möller-Trumbore
/* -------------------------------------------------------------------------- */
bool Model::rayTriangleIntersection(
	vec3 orig, vec3 dir, 
	const vec3 & v0, const vec3 & v1, const vec3 & v2, 
	double & u, double & v, double & t) 
{
    const float kEPSILON = 0.0000001;
    vec3 edge1, edge2, h, s, q;
    double a, f;

    edge1 = v1 - v0;
    edge2 = v2 - v0;
    h = cross(dir, edge2);
    a = dot(edge1, h);

    // le rayon est parallele au triangle
    if( a < kEPSILON) return false;
    //if (a > -kEPSILON && a < kEPSILON) return false;
    //if(nearlyEqual(a, 0.0, EPSILON) || nearlyEqual(a, 0.0, -EPSILON) || a > EPSILON) return false;
    f = 1.0/a;
    s = orig - v0;
    u = f * dot(s, h);

    if (u < 0.0 || u > 1.0) return false;

    q = cross(s, edge1);
    v = f * dot(dir, q);

    if (v < 0.0 || (u + v) > 1.0) return false;

    // on calcul une potentiel distance t 
    double tmp = f * dot(edge2, q);
    if(tmp > kEPSILON) // ok, il y a intersection
    {
    	t = tmp;
    	return true;
    } // intersection de droite, mais pas de rayon
    else return false;
}

/****************************************************************************/
bool Model::rayBoxIntersection(vec3 ray_orig, vec3 ray_dir, double & t, vec3 box_min, vec3 box_max)
{    
    float tmin = (box_min.x - ray_orig.x) / ray_dir.x;
    float tmax = (box_max.x - ray_orig.x) / ray_dir.x;
    
    if (tmin > tmax) std::swap(tmin, tmax);
        
    float tymin = (box_min.y - ray_orig.y) / ray_dir.y; 
    float tymax = (box_max.y - ray_orig.y) / ray_dir.y; 
 
    if (tymin > tymax) std::swap(tymin, tymax); 
    if ((tmin > tymax) || (tymin > tmax)) return false; 
    if (tymin > tmin) tmin = tymin; 
    if (tymax < tmax) tmax = tymax; 
 
    float tzmin = (box_min.z - ray_orig.z) / ray_dir.z; 
    float tzmax = (box_max.z - ray_orig.z) / ray_dir.z; 
 
    if (tzmin > tzmax) std::swap(tzmin, tzmax); 
    if ((tmin > tzmax) || (tzmin > tmax)) return false; 
    if (tzmin > tmin) tmin = tzmin; 
    if (tzmax < tmax) tmax = tzmax; 
 
    return true; 
}
