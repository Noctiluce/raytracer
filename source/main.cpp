#include <stdio.h>
#include <iomanip>
#include <time.h>

#include "common.h"
#include "SourcePath.h"
#include "stb_image.h"


#define NO_SHADOW 0
#define HARD_SHADOW 1
#define SOFT_SHADOW 2

using namespace Angel;
typedef vec4  color4;
typedef vec4  point4;
//Scene variables
enum { _SPHERE, _SQUARE, _BOX, _BUBBLE, _FUN};
int scene = _FUN;//Simple sphere, square or cornell box
std::vector < Object * > sceneObjects;
point4 lightPosition;
color4 lightColor;
point4 cameraPosition;
//Recursion depth for raytracer
int maxDepth = 3;
int shadow;
bool antialiazingON;
int IMG_WIDTH, IMG_HEIGHT;
namespace GLState { 
	int window_width, window_height; bool render_line; 
	std::vector < GLuint > objectVao; std::vector < GLuint > objectBuffer; 
	GLuint vPosition, vNormal, vTexCoord, program, ModelView, ModelViewLight, NormalMatrix, Projection;
    //==========Trackball Variables==========
	static float curquat[4], lastquat[4];
	/* current transformation matrix */
	static float curmat[4][4]; mat4 curmat_a;
	/* actual operation  */
	static int scaling; static int moving; static int panning;
	/* starting "moving" coordinates */
	static int beginx, beginy;
	/* ortho */
	float ortho_x, ortho_y;
	/* current scale factor */
	static float scalefactor; mat4  projection; mat4 sceneModelView; color4 light_ambient; color4 light_diffuse; color4 light_specular;
};
class rayTraceReceptor;
bool write_image(const char* filename, const unsigned char *Src,int Width, int Height, int channels) ;
std::vector < vec4 > findRay(GLdouble x, GLdouble y);
bool intersectionSort(Object::IntersectionValues i, Object::IntersectionValues j);
void castRayDebug(vec4 p0, vec4 dir);
void rayTrace();
static void error_callback(int error, const char* description);
void initCornellBox();
void initUnitSphere();
void initUnitSquare();
void initBubbleScene();
void initFunScene();
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void mouseClick(GLFWwindow* window, int button, int action, int mods);
void mouseMove(GLFWwindow* window, double x, double y);
void initGL();
void drawObject(Object * object, GLuint vao, GLuint buffer);


/* ******************************** TODO ************************************ */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */

vec4 x(vec4 v1, vec4 v2)
{
	vec4 result;
	result.x = v1.x * v2.x;
	result.y = v1.y * v2.y;
	result.z = v1.z * v2.z;
	result.w = v1.w * v2.w;
	return result;
}

inline float clamp(const float &lo, const float &hi, const float &v) { return std::max(lo, std::min(hi, v)); } 


inline vec4 mix(const vec4 &a, const vec4 &b, const float &mixValue) { return a * (1 - mixValue) + b * mixValue; } 
inline float mix (float x, float y, float a) {return x * (1 - a) + y * a ;}
inline vec3 mix (vec3 x, vec3 y, float a) {return x * (1 - a) + y * a ;}

float smoothstep(float edge0, float edge1, float x) {
  // Scale, bias and saturate x to 0..1 range
  x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0); 
  // Evaluate polynomial
  return x * x * (3 - 2 * x);
}

inline vec3 vec4tovec3(vec4 v){return vec3(v.x, v.y, v.z);}
inline vec4 reflectFunction(vec4 E, vec4 N){ return normalize(E - 2.0 * dot(E, N) * N); }
inline vec3 reflectFunction(vec3 E, vec3 N) { return normalize(E - 2.0 * dot(E, N) * N); }

vec4 refractFunction(vec4 I, vec4 N, const double & ior)
{
    double c1 = dot(vec3(N.x, N.y, N.z), vec3(I.x, I.y, I.z));
    
    double n1 = 1.000293;
    double n2 = ior;
    double n = n1 / n2;

    vec4 normal = N;
    if (dot(vec3(I.x, I.y, I.z), vec3(normal.x, normal.y, normal.z)) < 0.0) c1 = -c1;
    else normal = -normal, n = n2 / n1;
    
    auto c2 = sqrtf(1.0-(n*n)*(1.0 - c1*c1));
    return (n*I)+(n*c1-c2)*N*2.0;
}

void fresnelFunction(vec4 I, vec4 N, const float &ior, float &kr) 
{ 
    float cosi = clamp(-1, 1, dot(vec4tovec3(I),vec4tovec3(N))); 
    float etai = 1.000293, etat = ior; 
    if (cosi >= 0) { std::swap(etai, etat); } 
    float sint = etai / etat * sqrtf(std::max(0.f, 1.0f - cosi * cosi)); 
    // reflection interne total
    if (sint > 1) { 
        kr = 1; 
    } 
    else { 
        float cost = sqrtf(std::max(0.f, 1.0f - sint * sint)); 
        cosi = fabsf(cosi); 
        float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost)); 
        float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost)); 
        kr = (Rs * Rs + Rp * Rp) / 2.0; 
    } 
} 

bool shadowFeeler(vec4 p0, Object *object) 
{
	bool inShadow = false; // default : not in shadow

	// vec p0 -> light
    vec4 L = lightPosition - p0;
    double tmin = std::numeric_limits< double >::infinity();
    auto light = length(L); L.w = 0.0;
    L = normalize(L); 


    //#pragma omp parallel for
	for (Object * o : sceneObjects)
	{
        if(o == object) continue;
            
		auto hit = o->intersect(p0, L);
		double t = hit.t;
		if (t < tmin && t < light)
		{
			inShadow = true; // in shadow
            break;
		} 
	}
	return inShadow; 
}

// ------------

vec3 iridescent( float ramp_p ) 
// https://www.itp.uni-hannover.de/fileadmin/arbeitsgruppen/zawischa/static_html/twobeams.html 
{
    ramp_p = ramp_p - floor(ramp_p);	// force les valeurs entre 0 et 1
    vec3 col0, col1;
    
    // en se referant aux indice de refraction de la lumiere
    if( ramp_p < 0.05 ){col0 = vec3(0.33, 0.49, 0.50);col1 = vec3(0.27, 0.33, 0.48);}
    if( ramp_p >= 0.05 && ramp_p < 0.1 ) {col0 = vec3(0.27, 0.33, 0.48);col1 = vec3(0.74, 0.77, 0.81);}
    if( ramp_p >= 0.1 && ramp_p < 0.15 ) {col0 = vec3(0.74, 0.77, 0.81);col1 = vec3(0.81, 0.58, 0.21);}
    if( ramp_p >= 0.15 && ramp_p < 0.2 ) {col0 = vec3(0.81, 0.58, 0.21);col1 = vec3(0.37, 0.44, 0.13);}
    if( ramp_p >= 0.2 && ramp_p < 0.25 ) {col0 = vec3(0.37, 0.44, 0.13);col1 = vec3(0.00, 0.18, 0.72);}
    if( ramp_p >= 0.25 && ramp_p < 0.3 ) {col0 = vec3(0.00, 0.18, 0.72);col1 = vec3(0.27, 0.74, 0.59);}
    if( ramp_p >= 0.3 && ramp_p < 0.35 ) {col0 = vec3(0.27, 0.74, 0.59);col1 = vec3(0.87, 0.67, 0.16);}
    if( ramp_p >= 0.35 && ramp_p < 0.4 ) {col0 = vec3(0.87, 0.67, 0.16);col1 = vec3(0.89, 0.12, 0.43);}
    if( ramp_p >= 0.4 && ramp_p < 0.45 ) {col0 = vec3(0.89, 0.12, 0.43);col1 = vec3(0.11, 0.13, 0.80);}
    if( ramp_p >= 0.45 && ramp_p < 0.5 ) {col0 = vec3(0.11, 0.13, 0.80);col1 = vec3(0.00, 0.60, 0.28);}
    if( ramp_p >= 0.5 && ramp_p < 0.55 ) {col0 = vec3(0.00, 0.60, 0.28);col1 = vec3(0.55, 0.68, 0.15);}
    if( ramp_p >= 0.55 && ramp_p < 0.6 ) {col0 = vec3(0.55, 0.68, 0.15);col1 = vec3(1.00, 0.24, 0.62);}
    if( ramp_p >= 0.6 && ramp_p < 0.65 ) {col0 = vec3(1.00, 0.24, 0.62);col1 = vec3(0.53, 0.15, 0.59);}
    if( ramp_p >= 0.65 && ramp_p < 0.7 ) {col0 = vec3(0.53, 0.15, 0.59);col1 = vec3(0.00, 0.48, 0.21);}
    if( ramp_p >= 0.7 && ramp_p < 0.75 ) {col0 = vec3(0.00, 0.48, 0.21);col1 = vec3(0.18, 0.62, 0.38);}
    if( ramp_p >= 0.75 && ramp_p < 0.8 ) {col0 = vec3(0.18, 0.62, 0.38);col1 = vec3(0.80, 0.37, 0.59);}
    if( ramp_p >= 0.8 && ramp_p < 0.85 ) {col0 = vec3(0.80, 0.37, 0.59);col1 = vec3(0.77, 0.23, 0.39);}
    if( ramp_p >= 0.85 && ramp_p < 0.9 ) {col0 = vec3(0.77, 0.23, 0.39);col1 = vec3(0.27, 0.38, 0.32);}
    if( ramp_p >= 0.9 && ramp_p < 0.95 ) {col0 = vec3(0.27, 0.38, 0.32);col1 = vec3(0.10, 0.53, 0.50);}
    if( ramp_p >= 0.95 && ramp_p < 1. )  {col0 = vec3(0.10, 0.53, 0.50);col1 = vec3(0.33, 0.49, 0.50);}
    float bias = 1.-((ramp_p*20.) - floor(ramp_p*20.));
    bias = smoothstep(0., 1., bias);
    vec3 col = mix(col1, col0, bias);
    return vec3(pow(col.x,0.8), pow(col.y,0.8), pow(col.z,0.8));
}

vec3 soap_p( vec3 p )
// en s'inspirant divers travaux sur les bulles sur ShaderToy
{
    p *= 2.1276764; // frequence
 	float ct = (p.x * p.y * p.z) / 0.00675; // la position du film d'huile
	for(int i=1;i<115;i++)
	{
		vec3 newp = p;
		newp.x+=0.45/float(i)*cos(float(i)*p.y+(ct)*0.3/40.0+0.23*float(i))-432.6;
        newp.y+=0.45/float(i)*sin(float(i)*p.x+(ct)*0.3/50.0+0.23*float(i-66))+64.66;
        newp.z+=0.45/float(i)*cos(float(i)*p.x-p.y+(ct)*0.1/150.0+0.23*float(i+6))-56. + ct/320000.;
        p = newp;
	}
    vec3 col = vec3(0.5*sin(1.*p.x)+0.5, 0.5*sin(1.0*p.y)+0.5, 1.*sin(.8*p.z)+0.5);
    col = vec3( col.x + col.y + col.z ) / 3.; // pour la luminance
    
    return col;
}
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */

/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
// - cast Ray = p0 + t*dir and intersect with sphere return color
vec4 castRay(vec4 p0, vec4 E, Object *lastHitObject, int depth) {
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	if (depth > maxDepth) { return color; }

	double tmin = +std::numeric_limits< double >::infinity();
	Object * object = nullptr;
    
    //#pragma omp parallel for
	for (auto o : sceneObjects)
	{
		double t = o->intersect(p0, E).t;

		if (t < tmin && t > 0) // && o != lastHitObject)
		{
			tmin = t;
			color = o->shadingValues.color;
			object = o;
		}
	}

    if(object != nullptr)
    {
    	auto hit = object->intersect(p0, E);

    	// vec3 hit->light
    	vec4 Lt = lightPosition - hit.P; Lt.w = 0; 
    	vec3 L = normalize(vec3(Lt.x, Lt.y, Lt.z));
    	// vec3 hit->observer
    	vec4 Vt = -E; 
    	vec3 V = normalize(vec3(Vt.x, Vt.y, Vt.z));
    	// vec3 of normal of hit
    	vec3 N = normalize(vec3(hit.N.x, hit.N.y, hit.N.z));
    	// vec3 reflection
    	vec3 R = normalize(2.0f*dot(N, L)*N - L); 

    	// ambiante intensity
        vec4 colorLastObj = vec4(0.0);
        
    	vec4 ambient = x(GLState::light_ambient, 
    					 object->shadingValues.color * object->shadingValues.Ka);

    	// diffuse
    	float tmp = dot(L, N); if (tmp < 0.0) tmp = 0.0;
        vec4 diffuse = x(GLState::light_diffuse, 
    					 object->shadingValues.color * object->shadingValues.Kd * tmp);
        
    	
    	// specular
    	float t = dot(R, V); if(t < 0.0) t = 0.0f;
    	vec4 specular = x(GLState::light_specular, 
    				      object->shadingValues.color * object->shadingValues.Ks * powf(t, object->shadingValues.Kn));

   		// global color
   		float w = color.w;
    	//color = object->shadingValues.color;
        color = diffuse+ambient+specular;
    	
	    // soft shadow
        if (shadow == SOFT_SHADOW) {
            auto saveLight = lightPosition;
            
            int areaLightsize = 1; // 0.1 of size
            int numberOfRay = 50;
            srand (time(NULL));
            double coeff = 0.0;
            
            double wh = (areaLightsize/20.0);
            
        
            for (float width = -0.5; width <= 0.5 ; width += 0.2)
            {
                for (float height = -0.5; height <= 0.5 ; height += 0.2)
                {
                    double xr, zr;
                    xr = width + (rand() % 10) / 100.0 - 0.05;
                    zr = height + (rand() % 10) / 100.0 - 0.05;
                    lightPosition = saveLight - vec4(xr, 0, zr, 0);
                    if(shadowFeeler(hit.P, object)) coeff++;
                }
            }
            double c = ((double) coeff) / ((double) numberOfRay);
            color = color - c * vec4(0.35, 0.35, 0.35, 0.0);
            if(color.x > 1.0f) color.x = 1.0f;
            if(color.y > 1.0f) color.y = 1.0f;
            if(color.z > 1.0f) color.z = 1.0f; 

            if(color.x < 0.0f) color.x = 0.0f;
            if(color.y < 0.0f) color.y = 0.0f;
            if(color.z < 0.0f) color.z = 0.0f;
            color.w = w;
            
            lightPosition = saveLight;
        }
        else if (shadow == HARD_SHADOW)
        {
            if(shadowFeeler(hit.P, object)) color = color - vec4(0.075, 0.075, 0.075, 0.0);
            if(color.x > 1.0f) color.x = 1.0f;
            if(color.y > 1.0f) color.y = 1.0f;
            if(color.z > 1.0f) color.z = 1.0f; 

            if(color.x < 0.0f) color.x = 0.0f;
            if(color.y < 0.0f) color.y = 0.0f;
            if(color.z < 0.0f) color.z = 0.0f;
        }
        
        double biasCoef = 0.0001;
        vec4 bias = hit.N * biasCoef;
        bool outside = dot(vec4tovec3(E), vec4tovec3(hit.N)) < 0.0;

        if(!object->isBubble && object->shadingValues.Kt > 0 && object->shadingValues.Kr > 0)
        {
            vec4 refractionColor = vec4(0), reflectionColor = vec4(0);
            float kr;
            fresnelFunction(E, hit.N, object->shadingValues.Kr, kr);
            //kr = smoothstep(0., 1., kr);
            if (kr < 1.0)
            {
                vec4 refractionDir = normalize(refractFunction(E, hit.N, object->shadingValues.Kr));  
                vec4 refractionOrig = outside ? hit.P - bias : hit.P + bias ;
                refractionColor = castRay(refractionOrig, refractionDir, object, depth+1);
            }
            
            vec3 reflectionDirtmp = normalize(reflectFunction(vec4tovec3(E), vec4tovec3(hit.N)));
            vec4 reflectionDir = vec4(reflectionDirtmp.x, reflectionDirtmp.y, reflectionDirtmp.z, 0.0);
            vec4 reflectionOrig = outside ? hit.P + bias : hit.P - bias;
            reflectionColor = castRay(reflectionOrig, reflectionDir, object, depth+1);
            
            
            color = color * (1.0 - object->shadingValues.Kt) + (object->shadingValues.Kt) * ((reflectionColor * kr) + (refractionColor * (1.0 - kr))); 
        }
        else if(object->isBubble || object->shadingValues.Kt > 0)
        {
            vec3 reflectionDirtmp = normalize(reflectFunction(vec4tovec3(E), vec4tovec3(hit.N)));
            vec4 reflectionDir = vec4(reflectionDirtmp.x, reflectionDirtmp.y, reflectionDirtmp.z, 0.0);
            vec4 reflectionOrig = outside ? hit.P + bias : hit.P - bias;
            color = (1-object->shadingValues.Kt) * color + object->shadingValues.Kt*castRay(reflectionOrig, reflectionDir, object, depth+1);
        }
        else if(!object->isBubble && object->shadingValues.Kr > 0)
        {
            
           float kr;
           fresnelFunction(E, hit.N, object->shadingValues.Kr, kr);
            
            //std::cout << "1-kr = " << 1.0 - kr << std::endl;
           bool outside = dot(E, hit.N) < 0.0;
           vec4 bias = hit.N * biasCoef;
           
           vec4 refractionDir = normalize(refractFunction(E, hit.N, object->shadingValues.Kr));  
           //vec4 refractionDir = vec4(refractionDirtmp.x, refractionDirtmp.y, refractionDirtmp.z, 0.0);                 
           vec4 refractionOrig = outside ? hit.P - bias : hit.P + bias ;
           color = (1.0 - kr) * castRay(refractionOrig, refractionDir, object, depth+1);
            
        }
        if(object->isBubble){
            vec3 I = normalize(vec4tovec3(p0-hit.P)); 
            float fresnel = 1.-dot(vec4tovec3(hit.N), I);
            fresnel = pow(fresnel, 4.25);
            fresnel = fresnel + 0.075 * (1.0 - fresnel);
            
            vec3 spec = vec4tovec3(color);
            spec *= fresnel*0.5;
            
            vec3 soap_col = soap_p(vec4tovec3(hit.P));
            soap_col = iridescent(soap_col.x);	// associe a la couleur iridescente
            soap_col = soap_col * (2.0*vec4tovec3(color) + spec);
            
            vec4 fond = castRay(hit.P, E, object, depth+1);

            color.x = mix(fond.x, pow(soap_col.x,0.952), pow(fresnel, 0.85)); 
            color.y = mix(fond.y, pow(soap_col.y,0.952), pow(fresnel, 0.85));
            color.z = mix(fond.z, pow(soap_col.z,0.952), pow(fresnel, 0.85));
        }
        
    
        // ---
        color.w = w;

		if(color.x > 1.0f) color.x = 1.0f;
	    if(color.y > 1.0f) color.y = 1.0f;
	    if(color.z > 1.0f) color.z = 1.0f; 

		if(color.x < 0.0f) color.x = 0.0f;
	    if(color.y < 0.0f) color.y = 0.0f;
	    if(color.z < 0.0f) color.z = 0.0f;
	}

	return color ;	
}




/* ******************************** MAIN ************************************ */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
int main(void) {

	GLFWwindow* window;

	glfwSetErrorCallback(error_callback);

	if (!glfwInit())
		exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	glfwWindowHint(GLFW_SAMPLES, 4);

	window = glfwCreateWindow(256*2, 256*2, "Raytracer", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetKeyCallback(window, keyCallback);
	glfwSetMouseButtonCallback(window, mouseClick);
	glfwSetCursorPosCallback(window, mouseMove);


	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	glfwSwapInterval(1);

	switch (scene) {
	case _SPHERE:
		initUnitSphere();
		break;
	case _SQUARE:
		initUnitSquare();
		break;
	case _BOX:
		initCornellBox();
		break;
    case _BUBBLE:
        initBubbleScene();
        break;
    case _FUN:
        initFunScene();
        break;
    }

	initGL();
    
    
	while (!glfwWindowShouldClose(window)) {

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		GLState::window_height = height;
		GLState::window_width = width;

		glViewport(0, 0, width, height);


		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		mat4 track_ball = mat4(GLState::curmat[0][0], GLState::curmat[1][0],
			GLState::curmat[2][0], GLState::curmat[3][0],
			GLState::curmat[0][1], GLState::curmat[1][1],
			GLState::curmat[2][1], GLState::curmat[3][1],
			GLState::curmat[0][2], GLState::curmat[1][2],
			GLState::curmat[2][2], GLState::curmat[3][2],
			GLState::curmat[0][3], GLState::curmat[1][3],
			GLState::curmat[2][3], GLState::curmat[3][3]);

		GLState::sceneModelView = Translate(-cameraPosition) *   //Move Camera Back
			Translate(GLState::ortho_x, GLState::ortho_y, 0.0) *
			track_ball *                   //Rotate Camera
			Scale(GLState::scalefactor,
				GLState::scalefactor,
				GLState::scalefactor);   //User Scale

		GLfloat aspect = GLfloat(width) / height;

		switch (scene) {
		case _SPHERE:
		case _SQUARE:
			GLState::projection = Perspective(45.0, aspect, 0.01, 100.0);
			break;
		case _BOX:
			GLState::projection = Perspective(45.0, aspect, 4.5, 100.0);
            break;
        case _BUBBLE:
            GLState::projection = Perspective(45.0, aspect, 4.5, 100.0);
            break;
        case _FUN:
            GLState::projection = Perspective(45.0, aspect, 4.5, 100.0);
            break;
        }

		glUniformMatrix4fv(GLState::Projection, 1, GL_TRUE, GLState::projection);

		for (unsigned int i = 0; i < sceneObjects.size(); i++) {
			drawObject(sceneObjects[i], GLState::objectVao[i], GLState::objectBuffer[i]);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();

	}

	glfwDestroyWindow(window);

	glfwTerminate();
	exit(EXIT_SUCCESS);
}


/* ***************************** FUNCTIONS *********************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */
/* -------------------------------------------------------------------------- */
/* ************************************************************************** */


/* ------------------------------------------------------- */
/* -- PNG receptor class for use with pngdecode library -- */
class rayTraceReceptor : public cmps3120::png_receptor
{
private:
	const unsigned char *buffer;
	unsigned int width;
	unsigned int height;
	int channels;

public:
	rayTraceReceptor(const unsigned char *use_buffer,
		unsigned int width,
		unsigned int height,
		int channels) {
		this->buffer = use_buffer;
		this->width = width;
		this->height = height;
		this->channels = channels;
	}
	cmps3120::png_header get_header() {
		cmps3120::png_header header;
		header.width = width;
		header.height = height;
		header.bit_depth = 8;
		switch (channels)
		{
		case 1:
			header.color_type = cmps3120::PNG_GRAYSCALE; break;
		case 2:
			header.color_type = cmps3120::PNG_GRAYSCALE_ALPHA; break;
		case 3:
			header.color_type = cmps3120::PNG_RGB; break;
		default:
			header.color_type = cmps3120::PNG_RGBA; break;
		}
		return header;
	}
	cmps3120::png_pixel get_pixel(unsigned int x, unsigned int y, unsigned int level) {
		cmps3120::png_pixel pixel;
		unsigned int idx = y * width + x;
		/* pngdecode wants 16-bit color values */
		pixel.r = buffer[4 * idx] * 257;
		pixel.g = buffer[4 * idx + 1] * 257;
		pixel.b = buffer[4 * idx + 2] * 257;
		pixel.a = buffer[4 * idx + 3] * 257;
		return pixel;
	}
};

/* -------------------------------------------------------------------------- */
/* ------------  Ray trace our scene.  Output color to image and    --------- */
/* -----------   Output color to image and save to disk             --------- */
void rayTrace() {
	
    unsigned char *buffer = new unsigned char[GLState::window_width*GLState::window_height * 4];
    
    if (antialiazingON){
        const double MUL = 2.0;
        unsigned char *bufferBigSize = new unsigned char[(GLState::window_width * (int) MUL) * (GLState::window_height * (int) MUL) * 4];
        
        time_t start, end;
        time(&start);
        
        float size_img = (GLState::window_width *MUL)*(GLState::window_height);
        int progress = 0, lastprogress = 0;
        
        for (unsigned int i = 0; i < (GLState::window_width * MUL); i++) {
            for (unsigned int j = 0; j < (GLState::window_height * MUL); j++) {
                
                progress = floor(((i*GLState::window_width+j) / size_img) * 10000.0);
                
                if (progress != lastprogress){
                    std::cout << " " << (float) progress / 100.0f << " % complete" << std::endl; 
                    lastprogress = progress;
                }
                
                int idx = j * (GLState::window_width*MUL) + i;
                std::vector < vec4 > ray_o_dir = findRay(i/MUL, j/MUL);
                vec4 color = castRay(ray_o_dir[0], vec4(ray_o_dir[1].x, ray_o_dir[1].y, ray_o_dir[1].z, 0.0), NULL, 0);
                
                bufferBigSize[4 * idx] = color.x * 255;
                bufferBigSize[4 * idx + 1] = color.y * 255;
                bufferBigSize[4 * idx + 2] = color.z * 255;
                bufferBigSize[4 * idx + 3] = color.w * 255;
            }
        }
        for (unsigned int i = 0; i < GLState::window_width; i++) {
            for (unsigned int j = 0; j < GLState::window_height; j++) {

                int idx = j * (GLState::window_width) + i;
                int idx2 = j*MUL * (GLState::window_width*MUL) + i*MUL;

                buffer[4 * idx] = (   bufferBigSize[4 *  idx2      - 4] + bufferBigSize[4 *  idx2]      + bufferBigSize[4 *  idx2      + 4] 
                                    + bufferBigSize[4 * (idx2 - 1) - 4] + bufferBigSize[4 * (idx2 - 1)] + bufferBigSize[4 * (idx2 - 1) + 4]
                                    + bufferBigSize[4 * (idx2 + 1) - 4] + bufferBigSize[4 * (idx2 + 1)] + bufferBigSize[4 * (idx2 + 1) + 4]  
                                ) / 9.0;

                buffer[4 * idx + 1] = (  bufferBigSize[4 *  idx2   - 4  + 1] + bufferBigSize[4 *  idx2  + 1]      + bufferBigSize[4 *  idx2      + 4  + 1] 
                                    + bufferBigSize[4 * (idx2 - 1) - 4  + 1] + bufferBigSize[4 * (idx2 - 1) + 1] + bufferBigSize[4 * (idx2 - 1) + 4  + 1]
                                    + bufferBigSize[4 * (idx2 + 1) - 4  + 1] + bufferBigSize[4 * (idx2 + 1) + 1] + bufferBigSize[4 * (idx2 + 1) + 4 + 1]  
                                ) / 9.0;

                buffer[4 * idx + 2] = (  bufferBigSize[4 *  idx2   - 4  + 2] + bufferBigSize[4 *  idx2  + 2]      + bufferBigSize[4 *  idx2      + 4  + 2] 
                                    + bufferBigSize[4 * (idx2 - 1) - 4  + 2] + bufferBigSize[4 * (idx2 - 1)  + 2] + bufferBigSize[4 * (idx2 - 1) + 4  + 2]
                                    + bufferBigSize[4 * (idx2 + 1) - 4  + 2] + bufferBigSize[4 * (idx2 + 1)  + 2] + bufferBigSize[4 * (idx2 + 1) + 4  + 2]  
                                ) / 9.0;

                buffer[4 * idx + 3] = (  bufferBigSize[4 *  idx2   - 4  + 3] + bufferBigSize[4 *  idx2  + 3]      + bufferBigSize[4 *  idx2      + 4  + 3] 
                                    + bufferBigSize[4 * (idx2 - 1) - 4  + 3] + bufferBigSize[4 * (idx2 - 1)  + 3] + bufferBigSize[4 * (idx2 - 1) + 4  + 3]
                                    + bufferBigSize[4 * (idx2 + 1) - 4  + 3] + bufferBigSize[4 * (idx2 + 1)  + 3] + bufferBigSize[4 * (idx2 + 1) + 4  + 3]  
                                ) / 9.0;
            }
        }
        
        time(&end);
        double time_taken = double(end - start); 

        std::cout << "100.00 % complete" << std::endl; 
        std::cout << std::endl;
        std::cout << "Time taken by program is " << std::fixed 
            << time_taken << std::setprecision(5); 
        std::cout << " seconds. " << std::endl;
        
        write_image("output.png", buffer, GLState::window_width, GLState::window_height, 4);
        
        delete[] bufferBigSize;
    }
    else {
    
        time_t start, end;
        time(&start);
        
        float size_img = GLState::window_width * GLState::window_height;
        int progress = 0, lastprogress = 0;
        for (unsigned int i = 0; i < (GLState::window_width); i++) {
            for (unsigned int j = 0; j < (GLState::window_height); j++) {

                int idx = j * (GLState::window_width) + i;
                
                progress = floor(((i*GLState::window_width+j) / size_img) * 10000.0);
                
                if (progress != lastprogress){
                    std::cout << " " << (float) progress / 100.0f << " % complete" << std::endl; 
                    lastprogress = progress;
                }
                std::vector < vec4 > ray_o_dir = findRay(i, j);
                vec4 color = castRay(ray_o_dir[0], vec4(ray_o_dir[1].x, ray_o_dir[1].y, ray_o_dir[1].z, 0.0), NULL, 0);
                
                buffer[4 * idx] = color.x * 255;
                buffer[4 * idx + 1] = color.y * 255;
                buffer[4 * idx + 2] = color.z * 255;
                buffer[4 * idx + 3] = color.w * 255;
            }
        }
        time(&end);
        double time_taken = double(end - start); 

        std::cout << "100.00 % complete" << std::endl; 
        std::cout << std::endl;
        std::cout << "Time taken by program is " << std::fixed 
            << time_taken << std::setprecision(5); 
        std::cout << " seconds. " << std::endl;
        write_image("output.png", buffer, GLState::window_width, GLState::window_height, 4);
    }
	delete[] buffer;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void initCornellBox() {
	cameraPosition = point4(0.0, 0.0, 6.0, 1.0);
	lightPosition = point4(0.0, 1.5, 0.0, 1.0);
	lightColor = color4(1.0, 1.0, 1.0, 1.0);

	sceneObjects.clear();

    
	{ //Back Wall
		sceneObjects.push_back(new Square("Back Wall", Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Left Wall
		sceneObjects.push_back(new Square("Left Wall", RotateY(90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(255.0/255.0, 108/255.0, 180.0/255.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Right Wall
		sceneObjects.push_back(new Square("Right Wall", RotateY(-90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(147.0/255.0, 112/255.0, 219.0/255.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Floor
		sceneObjects.push_back(new Square("Floor", RotateX(-90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Ceiling
		sceneObjects.push_back(new Square("Ceiling", RotateX(90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Lamp
		sceneObjects.push_back(new Square("Lamp", RotateX(90)*Translate(0.0, 0.0, -1.999998)*Scale(1.0, 1.0, 0.5)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 1.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Front Wall
		sceneObjects.push_back(new Square("Front Wall", RotateY(180)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(0.0, 0.75, 0.5, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
    
    {
  sceneObjects.push_back(new Sphere("Glass sphere", vec3(1.0, -1.25, 0.5),0.75));
  Object::ShadingValues _shadingValues;
  _shadingValues.color = vec4(1.0,0.0,0.0,1.0);
  _shadingValues.Ka = 0.25;
  _shadingValues.Kd = 1.0;
  _shadingValues.Ks = 0.0;
  _shadingValues.Kn = 16.0;
  _shadingValues.Kt = 1.0;
  _shadingValues.Kr = 1.4;
  sceneObjects[sceneObjects.size()-1]->isBubble = false;
  sceneObjects[sceneObjects.size()-1]->setShadingValues(_shadingValues);
  sceneObjects[sceneObjects.size()-1]->setModelView(mat4());
  }
  
  {
  sceneObjects.push_back(new Sphere("Mirrored Sphere", vec3(-1.0, -1.25, -0.5),0.75));
  Object::ShadingValues _shadingValues;
  _shadingValues.color = vec4(0.8,1.0,1.0,1.0);
  _shadingValues.Ka = 0.25;
  _shadingValues.Kd = 1.0;
  _shadingValues.Ks = 1.0;
  _shadingValues.Kn = 16.0;
  _shadingValues.Kt = 1.0;
  _shadingValues.Kr = 0.0;
  sceneObjects[sceneObjects.size()-1]->isBubble = false;
  sceneObjects[sceneObjects.size()-1]->setShadingValues(_shadingValues);
  sceneObjects[sceneObjects.size()-1]->setModelView(mat4());
  }

	std::cout << "Total of objects : " << sceneObjects.size() << std::endl;
    //shadow = SOFT_SHADOW;
    //shadow = HARD_SHADOW;
    antialiazingON = true;
}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void initBubbleScene() {
    
    cameraPosition = point4(0.0, 0.0, 6.0, 1.0);
	lightPosition = point4(0.0, 1.5, 0.0, 1.0);
	lightColor = color4(1.0, 1.0, 1.0, 1.0);

	sceneObjects.clear();
    
    
	{ //Back Wall
		sceneObjects.push_back(new Square("Back Wall", Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Left Wall
		sceneObjects.push_back(new Square("Left Wall", RotateY(90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(255.0/255.0, 108/255.0, 180.0/255.0, 1.0);
		//_shadingValues.color = vec4(255.0/255.0, 192/255.0, 203.0/255.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Right Wall
		sceneObjects.push_back(new Square("Right Wall", RotateY(-90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(147.0/255.0, 112/255.0, 219.0/255.0, 1.0);
		//_shadingValues.color = vec4(177.0/255.0, 156.0/255.0, 217.0/255.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Floor
		sceneObjects.push_back(new Square("Floor", RotateX(-90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Ceiling
		sceneObjects.push_back(new Square("Ceiling", RotateX(90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Lamp
		sceneObjects.push_back(new Square("Lamp", RotateX(90)*Translate(0.0, 0.0, -1.999998)*Scale(1.0, 1.0, 0.5)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 1.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Front Wall
		sceneObjects.push_back(new Square("Front Wall", RotateY(180)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;

		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	const float bubble_ka = 1.0; //0.25
    const float bubble_kd = 1.0;
    const float bubble_ks = 1.0;
    const float bubble_kn = 16.0;
    const float bubble_kt = 1.0;
    const float bubble_kr = 0.0;
    const std::string bubble_name = "SoapBubble";
        
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(1.0, -1.25, 0.5), 0.5));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(-1.0, -1.25, -0.5), 0.5));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(0.0, -0.25, 1.0), 0.5));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(0.75, 1.0, 0.5), 0.5));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(-0.75, 0.75, -0.25), 0.5));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(1.2, -0.65, -0.75), 0.5));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	std::cout << "Total of objects : " << sceneObjects.size() << std::endl;


}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void initFunScene() {
    
    cameraPosition = point4(0.0, 0.0, 6.0, 1.0);
	lightPosition = point4(0.0, 1.5, 0.0, 1.0);
	lightColor = color4(1.0, 1.0, 1.0, 1.0);

	sceneObjects.clear();
    
    
	{ //Back Wall
		sceneObjects.push_back(new Square("Back Wall", Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Left Wall
		sceneObjects.push_back(new Square("Left Wall", RotateY(90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(255.0/255.0, 108/255.0, 180.0/255.0, 1.0);
		//_shadingValues.color = vec4(255.0/255.0, 192/255.0, 203.0/255.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Right Wall
		sceneObjects.push_back(new Square("Right Wall", RotateY(-90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(147.0/255.0, 112/255.0, 219.0/255.0, 1.0);
		//_shadingValues.color = vec4(177.0/255.0, 156.0/255.0, 217.0/255.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Floor
		sceneObjects.push_back(new Square("Floor", RotateX(-90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Ceiling
		sceneObjects.push_back(new Square("Ceiling", RotateX(90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Lamp
		sceneObjects.push_back(new Square("Lamp", RotateX(90)*Translate(0.0, 0.0, -1.999998)*Scale(1.0, 1.0, 0.5)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0, 1.0, 1.0, 1.0);
		_shadingValues.Ka = 1.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	{ //Front Wall
		sceneObjects.push_back(new Square("Front Wall", RotateY(180)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(0.0, 0.75, 0.5, 1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 0.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

	const float bubble_ka = 1.0; //0.25
    const float bubble_kd = 1.0;
    const float bubble_ks = 1.0;
    const float bubble_kn = 16.0;
    const float bubble_kt = 1.0;
    const float bubble_kr = 0.0;
    const std::string bubble_name = "SoapBubble";
       
  {
    //sceneObjects.push_back(new Sphere("Mirrored Sphere", vec3(0.675, 0.15, 0.75),0.75));
    sceneObjects.push_back(new Model(source_path + "/assets/3d_obj/suzanne.obj", Translate(0.675, 0.15, 0.75)*RotateY(-12)*Scale(0.65)));
    Object::ShadingValues _shadingValues;
    _shadingValues.color = vec4(255/255.0, 215/255.0, 0.0/255.0, 1.0);
    _shadingValues.Ka = 0.5;
    _shadingValues.Kd = 1.0;
    _shadingValues.Ks = 1.0;
    _shadingValues.Kn = 8.0;
    _shadingValues.Kt = 0.0;
    _shadingValues.Kr = 0.0;
    sceneObjects[sceneObjects.size()-1]->isBubble = false;
    sceneObjects[sceneObjects.size()-1]->setShadingValues(_shadingValues);
    sceneObjects[sceneObjects.size()-1]->setModelView(mat4());
  }
  
  
  
  {
  
  sceneObjects.push_back(new Model(source_path + "/assets/3d_obj/bunny.obj",  Translate(-0.5, -1.75, 0.0)*RotateY(25)*Scale(0.65)));
  Object::ShadingValues _shadingValues;
  _shadingValues.color = vec4(1.0,0.0,1.0,1.0);
  _shadingValues.Ka = 0.0;
  _shadingValues.Kd = 0.0;
  _shadingValues.Ks = 0.0;
  _shadingValues.Kn = 16.0;
  _shadingValues.Kt = 1.0;
  _shadingValues.Kr = 0.0;
  sceneObjects[sceneObjects.size()-1]->isBubble = false;
  sceneObjects[sceneObjects.size()-1]->setShadingValues(_shadingValues);
  sceneObjects[sceneObjects.size()-1]->setModelView(mat4());
  }
  
  {
        sceneObjects.push_back(new Sphere(bubble_name, vec3(0.75, 1.0, -0.5), 0.5));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(-1.1, 0.4, -0.68), 0.45));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(0.1, -0.4, -1.0), 0.8));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
	/*
	{
        sceneObjects.push_back(new Sphere(bubble_name, vec3(1.4, -0.7, 0.0), 0.2));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}*/
	
	{
  //sceneObjects.push_back(new Model(source_path + "/assets/3d_obj/bunny.obj", Translate(0.3, -1.75, 0.0)*Scale(0.75)));
	    sceneObjects.push_back(new Model(source_path + "/assets/3d_obj/teapot.obj", Translate(-0.75, 1.125, 0.75)*Scale(0.05)));
	  	Object::ShadingValues _shadingValues;
	  	_shadingValues.color = vec4(1.0);
	  	_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt/2.0; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
	  	sceneObjects[sceneObjects.size()-1]->isBubble = true;
	  	sceneObjects[sceneObjects.size()-1]->setShadingValues(_shadingValues);
	  	sceneObjects[sceneObjects.size()-1]->setModelView(mat4());
	  }/*
  {
        sceneObjects.push_back(new Sphere(bubble_name, vec3(0.35, -1.3, 1.0), 0.25));
        Object::ShadingValues _shadingValues;
		_shadingValues.color = vec4(1.0);
		_shadingValues.Ka = bubble_ka;
		_shadingValues.Kd = bubble_kd;
		_shadingValues.Ks = bubble_ks;
		_shadingValues.Kn = bubble_kn;
		_shadingValues.Kt = bubble_kt; // refraction
		_shadingValues.Kr = bubble_kr; // niveau de la matiere
		sceneObjects[sceneObjects.size() -1]->isBubble = true;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}*/
	{
        sceneObjects.push_back(new Model(source_path + "/assets/3d_obj/cow.obj",  Translate(1.125, -1.5, 0.5)*RotateY(110)*Scale(2.0)));
        Object::ShadingValues _shadingValues;
        _shadingValues.color = vec4(0.0,0.0,1.0,1.0);
        _shadingValues.Ka = 0.25;
        _shadingValues.Kd = 1.0;
        _shadingValues.Ks = 0.5;
        _shadingValues.Kn = 16.0;
        _shadingValues.Kt = 1.0; // a quel point c'est reflectif [0, 1]
        _shadingValues.Kr = 1.4; // l'indice de refraction
        sceneObjects[sceneObjects.size()-1]->isBubble = false;
        sceneObjects[sceneObjects.size()-1]->setShadingValues(_shadingValues);
        sceneObjects[sceneObjects.size()-1]->setModelView(mat4());
    }
   
	std::cout << "Total of objects : " << sceneObjects.size() << std::endl;
    
    shadow = SOFT_SHADOW;
   // shadow = HARD_SHADOW;
    antialiazingON = true;
}

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

void initUnitSphere() {
	cameraPosition = point4(0.0, 0.0, 3.0, 1.0);
	lightPosition = point4(0.0, 0.0, 4.0, 1.0);
	lightColor = color4(1.0, 1.0, 1.0, 1.0);

	sceneObjects.clear();

	{
		sceneObjects.push_back(new Sphere("Mirrored Sphere", vec3(-1.0, 0.0, -1.0)));
		Object::ShadingValues _shadingValues;
		//_shadingValues.color = vec4(1.0, 0.7529, 0.7961, 1.0);
		_shadingValues.color = vec4(0.9764, 0.2588, 0.6196, 1.0);
		_shadingValues.Ka = 0.25;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 1.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));

		sceneObjects.push_back(new Sphere("Mirrored Sphere", vec3(-1.0, 0.0, 1.0)));
	//	Object::ShadingValues _shadingValues;
		//_shadingValues.color = vec4(1.0, 0.7529, 0.7961, 1.0);
		_shadingValues.color = vec4(0.1, 0.7, 0.3, 1.0);
		_shadingValues.Ka = 0.25;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 1.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void initUnitSquare() {
	cameraPosition = point4(0.0, 0.0, 3.0, 1.0);
	lightPosition = point4(0.0, 0.0, 4.0, 1.0);
	lightColor = color4(1.0, 1.0, 1.0, 1.0);

	sceneObjects.clear();

	{ //Back Wall
		sceneObjects.push_back(new Square("Unit Square", RotateX(-90)*Translate(0.0, 0.0, -2.0)*Scale(2.0, 2.0, 1.0)));
		Object::ShadingValues _shadingValues;
  _shadingValues.color = vec4(0.8,1.0,1.0,1.0);
		_shadingValues.Ka = 0.0;
		_shadingValues.Kd = 1.0;
		_shadingValues.Ks = 1.0;
		_shadingValues.Kn = 16.0;
		_shadingValues.Kt = 0.0;
		_shadingValues.Kr = 0.0;
          sceneObjects[sceneObjects.size()-1]->isBubble = false;
		sceneObjects[sceneObjects.size() - 1]->setShadingValues(_shadingValues);
		sceneObjects[sceneObjects.size() - 1]->setModelView(mat4(1.0f));
	}

}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
		scene = _SPHERE;
		initUnitSphere();
		initGL();
	}
	if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
		scene = _SQUARE;
		initUnitSquare();
		initGL();
	}
	if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
		scene = _BOX;
		initCornellBox();
		initGL();
	}
	if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
        scene = _BUBBLE;
        initBubbleScene();
        initGL();
    }
    if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
        scene = _FUN;
        initFunScene();
        initGL();
    }
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
		rayTrace();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
static void mouseClick(GLFWwindow* window, int button, int action, int mods) {

	if (GLFW_RELEASE == action) {
		GLState::moving = GLState::scaling = GLState::panning = false;
		return;
	}

	if (mods & GLFW_MOD_SHIFT) {
		GLState::scaling = true;
	}
	else if (mods & GLFW_MOD_ALT) {
		GLState::panning = true;
	}
	else {
		GLState::moving = true;
		TrackBall::trackball(GLState::lastquat, 0, 0, 0, 0);
	}

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	GLState::beginx = xpos; GLState::beginy = ypos;

	std::vector < vec4 > ray_o_dir = findRay(xpos, ypos);
	castRayDebug(ray_o_dir[0], vec4(ray_o_dir[1].x, ray_o_dir[1].y, ray_o_dir[1].z, 0.0));

}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void mouseMove(GLFWwindow* window, double x, double y) {

	int W, H;
	glfwGetFramebufferSize(window, &W, &H);


	float dx = (x - GLState::beginx) / (float)W;
	float dy = (GLState::beginy - y) / (float)H;

	if (GLState::panning)
	{
		GLState::ortho_x += dx;
		GLState::ortho_y += dy;

		GLState::beginx = x; GLState::beginy = y;
		return;
	}
	else if (GLState::scaling)
	{
		GLState::scalefactor *= (1.0f + dx);

		GLState::beginx = x; GLState::beginy = y;
		return;
	}
	else if (GLState::moving)
	{
		TrackBall::trackball(GLState::lastquat,
			(2.0f * GLState::beginx - W) / W,
			(H - 2.0f * GLState::beginy) / H,
			(2.0f * x - W) / W,
			(H - 2.0f * y) / H
		);

		TrackBall::add_quats(GLState::lastquat, GLState::curquat, GLState::curquat);
		TrackBall::build_rotmatrix(GLState::curmat, GLState::curquat);

		GLState::beginx = x; GLState::beginy = y;
		return;
	}
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void initGL() {

	GLState::light_ambient = vec4(lightColor.x, lightColor.y, lightColor.z, 1.0);
	GLState::light_diffuse = vec4(lightColor.x, lightColor.y, lightColor.z, 1.0);
	GLState::light_specular = vec4(lightColor.x, lightColor.y, lightColor.z, 1.0);


	std::string vshader = source_path + "/assets/shaders/vshader.glsl";
	std::string fshader = source_path + "/assets/shaders/fshader.glsl";

	GLchar* vertex_shader_source = readShaderSource(vshader.c_str());
	GLchar* fragment_shader_source = readShaderSource(fshader.c_str());

	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, (const GLchar**)&vertex_shader_source, NULL);
	glCompileShader(vertex_shader);
	check_shader_compilation(vshader, vertex_shader);

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, (const GLchar**)&fragment_shader_source, NULL);
	glCompileShader(fragment_shader);
	check_shader_compilation(fshader, fragment_shader);

	GLState::program = glCreateProgram();
	glAttachShader(GLState::program, vertex_shader);
	glAttachShader(GLState::program, fragment_shader);

	glLinkProgram(GLState::program);
	check_program_link(GLState::program);

	glUseProgram(GLState::program);

	glBindFragDataLocation(GLState::program, 0, "fragColor");

	// set up vertex arrays
	GLState::vPosition = glGetAttribLocation(GLState::program, "vPosition");
	GLState::vNormal = glGetAttribLocation(GLState::program, "vNormal");

	// Retrieve transformation uniform variable locations
	GLState::ModelView = glGetUniformLocation(GLState::program, "ModelView");
	GLState::NormalMatrix = glGetUniformLocation(GLState::program, "NormalMatrix");
	GLState::ModelViewLight = glGetUniformLocation(GLState::program, "ModelViewLight");
	GLState::Projection = glGetUniformLocation(GLState::program, "Projection");

	GLState::objectVao.resize(sceneObjects.size());
	glGenVertexArrays(sceneObjects.size(), &GLState::objectVao[0]);

	GLState::objectBuffer.resize(sceneObjects.size());
	glGenBuffers(sceneObjects.size(), &GLState::objectBuffer[0]);

	for (unsigned int i = 0; i < sceneObjects.size(); i++) {
		glBindVertexArray(GLState::objectVao[i]);
		glBindBuffer(GL_ARRAY_BUFFER, GLState::objectBuffer[i]);
		size_t vertices_bytes = sceneObjects[i]->mesh.vertices.size() * sizeof(vec4);
		size_t normals_bytes = sceneObjects[i]->mesh.normals.size() * sizeof(vec3);

		glBufferData(GL_ARRAY_BUFFER, vertices_bytes + normals_bytes, NULL, GL_STATIC_DRAW);
		size_t offset = 0;
		glBufferSubData(GL_ARRAY_BUFFER, offset, vertices_bytes, &sceneObjects[i]->mesh.vertices[0]);
		offset += vertices_bytes;
		glBufferSubData(GL_ARRAY_BUFFER, offset, normals_bytes, &sceneObjects[i]->mesh.normals[0]);

		glEnableVertexAttribArray(GLState::vNormal);
		glEnableVertexAttribArray(GLState::vPosition);

		glVertexAttribPointer(GLState::vPosition, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
		glVertexAttribPointer(GLState::vNormal, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(vertices_bytes));

	}



	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);

	glClearColor(0.8, 0.8, 1.0, 1.0);

	//Quaternion trackball variables, you can ignore
	GLState::scaling = 0;
	GLState::moving = 0;
	GLState::panning = 0;
	GLState::beginx = 0;
	GLState::beginy = 0;

	TrackBall::matident(GLState::curmat);
	TrackBall::trackball(GLState::curquat, 0.0f, 0.0f, 0.0f, 0.0f);
	TrackBall::trackball(GLState::lastquat, 0.0f, 0.0f, 0.0f, 0.0f);
	TrackBall::add_quats(GLState::lastquat, GLState::curquat, GLState::curquat);
	TrackBall::build_rotmatrix(GLState::curmat, GLState::curquat);

	GLState::scalefactor = 1.0;
	GLState::render_line = false;

}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void drawObject(Object * object, GLuint vao, GLuint buffer) {

	color4 material_ambient(object->shadingValues.color.x*object->shadingValues.Ka,
		object->shadingValues.color.y*object->shadingValues.Ka,
		object->shadingValues.color.z*object->shadingValues.Ka, 1.0);
	color4 material_diffuse(object->shadingValues.color.x,
		object->shadingValues.color.y,
		object->shadingValues.color.z, 1.0);
	color4 material_specular(object->shadingValues.Ks,
		object->shadingValues.Ks,
		object->shadingValues.Ks, 1.0);
	float  material_shininess = object->shadingValues.Kn;

	color4 ambient_product = GLState::light_ambient * material_ambient;
	color4 diffuse_product = GLState::light_diffuse * material_diffuse;
	color4 specular_product = GLState::light_specular * material_specular;

	glUniform4fv(glGetUniformLocation(GLState::program, "AmbientProduct"), 1, ambient_product);
	glUniform4fv(glGetUniformLocation(GLState::program, "DiffuseProduct"), 1, diffuse_product);
	glUniform4fv(glGetUniformLocation(GLState::program, "SpecularProduct"), 1, specular_product);
	glUniform4fv(glGetUniformLocation(GLState::program, "LightPosition"), 1, lightPosition);
	glUniform1f(glGetUniformLocation(GLState::program, "Shininess"), material_shininess);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glVertexAttribPointer(GLState::vPosition, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glVertexAttribPointer(GLState::vNormal, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(object->mesh.vertices.size() * sizeof(vec4)));

	mat4 objectModelView = GLState::sceneModelView*object->getModelView();


	glUniformMatrix4fv(GLState::ModelViewLight, 1, GL_TRUE, GLState::sceneModelView);
	glUniformMatrix3fv(GLState::NormalMatrix, 1, GL_TRUE, Normal(objectModelView));
	glUniformMatrix4fv(GLState::ModelView, 1, GL_TRUE, objectModelView);

	glDrawArrays(GL_TRIANGLES, 0, object->mesh.vertices.size());

}



/* -------------------------------------------------------------------------- */
/* ----------------------  Write Image to Disk  ----------------------------- */
bool write_image(const char* filename, const unsigned char *Src,
	int Width, int Height, int channels) {
	cmps3120::png_encoder the_encoder;
	cmps3120::png_error result;
	rayTraceReceptor image(Src, Width, Height, channels);
	the_encoder.set_receptor(&image);
	result = the_encoder.write_file(filename);
	if (result == cmps3120::PNG_DONE)
		std::cerr << "finished writing " << filename << "." << std::endl;
	else
		std::cerr << "write to " << filename << " returned error code " << result << "." << std::endl;
	return result == cmps3120::PNG_DONE;
}


/* -------------------------------------------------------------------------- */
/* -------- Given OpenGL matrices find ray in world coordinates of ---------- */
/* -------- window position x,y --------------------------------------------- */
std::vector < vec4 > findRay(GLdouble x, GLdouble y) {

	y = GLState::window_height - y;

	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	GLdouble modelViewMatrix[16];
	GLdouble projectionMatrix[16];
	for (unsigned int i = 0; i < 4; i++) {
		for (unsigned int j = 0; j < 4; j++) {
			modelViewMatrix[j * 4 + i] = GLState::sceneModelView[i][j];
			projectionMatrix[j * 4 + i] = GLState::projection[i][j];
		}
	}


	GLdouble nearPlaneLocation[3];
	_gluUnProject(x, y, 0.0, modelViewMatrix, projectionMatrix,
		viewport, &nearPlaneLocation[0], &nearPlaneLocation[1],
		&nearPlaneLocation[2]);

	GLdouble farPlaneLocation[3];
	_gluUnProject(x, y, 1.0, modelViewMatrix, projectionMatrix,
		viewport, &farPlaneLocation[0], &farPlaneLocation[1],
		&farPlaneLocation[2]);


	vec4 ray_origin = vec4(nearPlaneLocation[0], nearPlaneLocation[1], nearPlaneLocation[2], 1.0);
	vec3 temp = vec3(farPlaneLocation[0] - nearPlaneLocation[0],
		farPlaneLocation[1] - nearPlaneLocation[1],
		farPlaneLocation[2] - nearPlaneLocation[2]);
	temp = normalize(temp);
	vec4 ray_dir = vec4(temp.x, temp.y, temp.z, 0.0);

	std::vector < vec4 > result(2);
	result[0] = ray_origin;
	result[1] = ray_dir;

	return result;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
bool intersectionSort(Object::IntersectionValues i, Object::IntersectionValues j) {
	return (i.t < j.t);
}

/* -------------------------------------------------------------------------- */
/* ---------  Some debugging code: cast Ray = p0 + t*dir  ------------------- */
/* ---------  and print out what it hits =                ------------------- */
void castRayDebug(vec4 p0, vec4 dir) {

	std::vector < Object::IntersectionValues > intersections;

	for (unsigned int i = 0; i < sceneObjects.size(); i++) {
		intersections.push_back(sceneObjects[i]->intersect(p0, dir));
		intersections[intersections.size() - 1].ID_ = i;
	}

	for (unsigned int i = 0; i < intersections.size(); i++) {
		if (intersections[i].t != std::numeric_limits< double >::infinity()) {
			std::cout << "Hit " << intersections[i].name << " " << intersections[i].ID_ << "\n";
			std::cout << "P: " << intersections[i].P << "\n";
			std::cout << "N: " << intersections[i].N << "\n";
			vec4 L = lightPosition - intersections[i].P;
			L = normalize(L);
			std::cout << "L: " << L << "\n";
		}
	}

}
