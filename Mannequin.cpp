//  ========================================================================
//  COSC422: Advanced Computer Graphics;  University of Canterbury (2019)
//
//  FILE NAME: ModelLoader.cpp
//
//  Press key '1' to toggle 90 degs model rotation about x-axis on/off.
//  ========================================================================

#include <iostream>
#include <map>
#include <GL/freeglut.h>
#include <IL/il.h>
using namespace std;

#include <assimp/cimport.h>
#include <assimp/types.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "assimp_extras.h"

//----------Globals----------------------------
const aiScene* scene = NULL;
aiAnimation* anim = NULL;
float angle = 0;
float camera_z = 3;
float speed = 0;
int floor_x = 0;
aiVector3D scene_min, scene_max, scene_center;
bool modelRotn = true;
float rotate_speed = 0;
std::map<int, int> texIdMap;

int tDuration; //Animation duration in ticks.
int currTick = 0; //current tick
float timeStep = 50; //Animation time step = 50 m.sec.

struct meshInit
{
	int mNumVertices;
	aiVector3D* mVertices;
	aiVector3D* mNormals;
};
meshInit* initData;

//------------Modify the following as needed----------------------
float materialCol[4] = { 0.9, 0.9, 0.9, 1 }; //Default material colour (not used if model's colour is available)
bool replaceCol = false; //Change to 'true' to set the model's colour to the above colour
float lightPosn[4] = { 0, 50, 50, 1 }; //Default light's position
bool twoSidedLight = false; //Change to 'true' to enable two-sided lighting

//-------Loads model data from file and creates a scene object----------
bool loadModel(const char* fileName)
{
    scene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality);
    anim = aiImportFile("run.fbx", aiProcessPreset_TargetRealtime_MaxQuality)->mAnimations[0];
    if (scene == NULL) exit(1);
	tDuration = anim->mDuration;
	cout << "HELP" << endl;
	
	aiMesh* mesh;
	int numVert;
	initData = new meshInit[scene->mNumMeshes];
	for (int m = 0; m < scene->mNumMeshes; m++)
	{
		mesh = scene->mMeshes[m];
		numVert = mesh->mNumVertices;
		(initData + m)->mNumVertices = numVert;
		(initData + m)->mVertices = new aiVector3D[numVert];
		(initData + m)->mNormals = new aiVector3D[numVert];
		
		for (int n = 0; n < numVert; n++) {
			(initData + m)->mVertices[n] = mesh->mVertices[n];
			(initData + m)->mNormals[n] = mesh->mNormals[n];
		}
	}
	
    //~ printSceneInfo(scene);
    //~ printMeshInfo(scene);
    //~ printTreeInfo(scene->mRootNode);
    //~ printBoneInfo(scene);
    //~ printAnimInfo(scene);  //WARNING:  This may generate a lengthy output if the model has animation data
    get_bounding_box(scene, &scene_min, &scene_max);
    return true;
}

//-------------Loads texture files using DevIL library-------------------------------
void loadGLTextures(const aiScene* scene)
{
    /* initialization of DevIL */
    ilInit();
    if (scene->HasTextures()) {
        std::cout << "Support for meshes with embedded textures is not implemented" << endl;
        return;
    }

    /* scan scene's materials for textures */
    /* Simplified version: Retrieves only the first texture with index 0 if present*/
    for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {
        aiString path; // filename

        if (scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
			const char* c = &path.data[path.length - 1];
			while (*(c-1) != '/' && c != path.data)
			{
				c--;
			}
            glEnable(GL_TEXTURE_2D);
            ILuint imageId;
            GLuint texId;
            ilGenImages(1, &imageId);
            glGenTextures(1, &texId);
            texIdMap[m] = texId; //store tex ID against material id in a hash map
            ilBindImage(imageId); /* Binding of DevIL image name */
            ilEnable(IL_ORIGIN_SET);
            ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
            if (ilLoadImage((ILstring)c)) //if success
            {
                /* Convert image to RGBA */
                ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

                /* Create and load textures to OpenGL */
                glBindTexture(GL_TEXTURE_2D, texId);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH),
                    ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    ilGetData());
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                cout << "Texture:" << c << " successfully loaded." << endl;
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
                glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            }
            else {
                cout << "Couldn't load Image: " << c << endl;
            }
        }
    } //loop for material
}

// ------A recursive function to traverse scene graph and render each mesh----------
void render(const aiScene* sc, const aiNode* nd)
{
    aiMatrix4x4 m = nd->mTransformation;
    aiMesh* mesh;
    aiFace* face;
    aiMaterial* mtl;
    GLuint texId;
    aiColor4D diffuse;
    int meshIndex, materialIndex;

    aiTransposeMatrix4(&m); //Convert to column-major order
    glPushMatrix();
    glMultMatrixf((float*)&m); //Multiply by the transformation matrix for this node

    // Draw all meshes assigned to this node
    for (int n = 0; n < nd->mNumMeshes; n++) {
        meshIndex = nd->mMeshes[n]; //Get the mesh indices from the current node
        mesh = scene->mMeshes[meshIndex]; //Using mesh index, get the mesh object

        materialIndex = mesh->mMaterialIndex; //Get material index attached to the mesh
        mtl = sc->mMaterials[materialIndex];
        if (replaceCol)
            glColor4fv(materialCol); //User-defined colour
        else if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) //Get material colour from model
            glColor4f(diffuse.r, diffuse.g, diffuse.b, 1.0);
        else
            glColor4fv(materialCol); //Default material colour

        if (mesh->HasTextureCoords(0)) {
            texId = texIdMap[mesh->mMaterialIndex];
            glBindTexture(GL_TEXTURE_2D, texId);
        }

        //Get the polygons from each mesh and draw them
        for (int k = 0; k < mesh->mNumFaces; k++) {
            face = &mesh->mFaces[k];
            GLenum face_mode;

            switch (face->mNumIndices) {
            case 1:
                face_mode = GL_POINTS;
                break;
            case 2:
                face_mode = GL_LINES;
                break;
            case 3:
                face_mode = GL_TRIANGLES;
                break;
            default:
                face_mode = GL_POLYGON;
                break;
            }

            glBegin(face_mode);

            for (int i = 0; i < face->mNumIndices; i++) {
                int vertexIndex = face->mIndices[i];

                if (mesh->HasVertexColors(0))
                    glColor4fv((GLfloat*)&mesh->mColors[0][vertexIndex]);

                if (mesh->HasTextureCoords(0)) {
                    glTexCoord2f(mesh->mTextureCoords[0][vertexIndex].x,
                        mesh->mTextureCoords[0][vertexIndex].y);
                }

                if (mesh->HasNormals())
                    glNormal3fv(&mesh->mNormals[vertexIndex].x);

                glVertex3fv(&mesh->mVertices[vertexIndex].x);
            }

            glEnd();
        }
    }

    // Draw all children
    for (int i = 0; i < nd->mNumChildren; i++)
        render(sc, nd->mChildren[i]);

    glPopMatrix();
}

// Update node vertices in character animation sequence

void updateNodeMatrices(int tick)
{
    int index;
    aiMatrix4x4 matPos, matRot, matProd;
    aiMatrix3x3 matRot3;
    aiNode* nd;
    for (int i = 0; i < anim->mNumChannels; i++) {
		if(i == 23) continue;
        matPos = aiMatrix4x4();
        //Identity
        matRot = aiMatrix4x4();
        aiNodeAnim* ndAnim = anim->mChannels[i]; //Channel
        if (ndAnim->mNumPositionKeys > 1)
            index = tick;
        else
            index = 0;
        aiVector3D posn = (ndAnim->mPositionKeys[index]).mValue;

        matPos.Translation(posn, matPos);
        if (ndAnim->mNumRotationKeys > 1)
            index = tick;
        else
            index = 0;
        aiQuaternion rotn = (ndAnim->mRotationKeys[index]).mValue;
        matRot3 = rotn.GetMatrix();
        matRot = aiMatrix4x4(matRot3);

        matProd = matPos * matRot;
        nd = scene->mRootNode->FindNode(ndAnim->mNodeName);
        if (nd == scene->mRootNode) cout << i << endl;
        cout << nd->mName.C_Str() << i << endl;
        nd->mTransformation = matProd;
    }
}

// Transform vertices of character models
void transformVertices()
{
	aiMesh* mesh;
	aiBone* bone;
	aiMatrix4x4 offset;
	aiMatrix4x4 m;
	aiMatrix4x4 normal;
	aiNode* nd;
	aiNode* parent;
	aiVector3D vert;
	aiVector3D norm;
	
	int vid;
	float weight;
	
	for (int n = 0; n < scene->mNumMeshes; n++) {
        mesh = scene->mMeshes[n]; //Get the mesh object
		aiMatrix4x4* transforms = (aiMatrix4x4*) calloc(mesh->mNumVertices, sizeof(aiMatrix4x4));
		aiMatrix4x4* normals = (aiMatrix4x4*) calloc(mesh->mNumVertices, sizeof(aiMatrix4x4));
		for (int b = 0; b < mesh->mNumBones; b++)
		{
			bone = mesh->mBones[b];
			offset = bone->mOffsetMatrix;
			nd = scene->mRootNode->FindNode(bone->mName);
			parent = nd->mParent;
			m = nd->mTransformation * offset;
			
			while (parent != NULL)
			{
				m = parent->mTransformation * m;
				parent = parent->mParent;
			}
			
			normal = m;
			normal.Inverse().Transpose();
			
			for (int w = 0; w < bone->mNumWeights; w++) {
				vid = (bone->mWeights[w]).mVertexId;
				weight = (bone->mWeights[w]).mWeight;
				
				transforms[vid] = transforms[vid] + m * weight;
				normals[vid] = normals[vid] + normal * weight;
			}
			
			
		}
		for (int i = 0; i < mesh->mNumVertices; i++)
		{
			vert = (initData + n)->mVertices[i];
			mesh->mVertices[i] = transforms[i] * vert;
			norm = (initData + n)->mNormals[i];
			mesh->mNormals[i] = normals[i] * vert;
		}
	}
}

//--------------------OpenGL initialization------------------------
void initialise()
{
    float ambient[4] = { 0.2, 0.2, 0.2, 1.0 }; //Ambient light
    float white[4] = { 1, 1, 1, 1 }; //Light's colour
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
    if (twoSidedLight)
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50);
    glColor4fv(materialCol);
    loadModel("mannequin.fbx"); //<<<-------------Specify input file name here
    loadGLTextures(scene);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(35, 1, 0.1, 1000.0);
}

//----Timer callback for continuous rotation of the model about y-axis----
void update(int value)
{
	floor_x = (floor_x - 5) % 100;
    angle += rotate_speed;
    camera_z += speed;
    if (angle > 360)
        angle = 0;
    if (currTick < tDuration) {
        updateNodeMatrices(currTick);
        transformVertices();
        currTick = (currTick + 1) % tDuration;
    }

    glutPostRedisplay();
    glutTimerFunc(50, update, 0);
}

//----Keyboard callback to toggle initial model orientation---
void keyboard(unsigned char key, int x, int y)
{
    if (key == '1')
        modelRotn = !modelRotn; //Enable/disable initial model rotation
    glutPostRedisplay();
}

void specialDown(int key, int x, int y)
{
	if (key == GLUT_KEY_LEFT)
	{
		rotate_speed = -0.1;
	}
	else if (key == GLUT_KEY_RIGHT)
	{
		rotate_speed = 0.1;
	}
	else if (key == GLUT_KEY_UP)
	{
		speed = -10;
	}
	else if (key == GLUT_KEY_DOWN)
	{
		speed = 10;
	}
}

void specialUp(int key, int x, int y)
{
	if (key == GLUT_KEY_LEFT || key == GLUT_KEY_RIGHT)
	{
		rotate_speed = 0;
	}
	else if (key == GLUT_KEY_UP || key == GLUT_KEY_DOWN)
	{
		speed = 0;
	}
}

void drawFloor()
{
    bool flag = false;

    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    for(int x = -5000; x <= 5000; x += 50)
    {
        for(int z = -5000; z <= 5000; z += 50)
        {
            if(flag) glColor3f(0.0, 0.0, 0.0);
            else glColor3f(1.0, 1.0, 1.0);
            glVertex3f(x + floor_x, 10, z);
            glVertex3f(x + floor_x, 10, z+50);
            glVertex3f(x+50 + floor_x, 10, z+50);
            glVertex3f(x+50 + floor_x, 10, z);
            flag = !flag;
        }
    }
    glEnd();
}

//------The main display function---------
//----The model is first drawn using a display list so that all GL commands are
//    stored for subsequent display updates.
void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float anim_z = anim->mChannels[0]->mPositionKeys[currTick].mValue.z;
    float anim_x = anim->mChannels[0]->mPositionKeys[currTick].mValue.x;
    gluLookAt(camera_z * sin(angle), 0, camera_z * cos(angle), 0, 0, 0, 0, 1, 0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosn);

    if (modelRotn)
        glRotatef(90, 1, 0, 0); //First, rotate the model about x-axis if needed.

    // scale the whole asset to fit into our view frustum
    float tmp = scene_max.x - scene_min.x;
    tmp = aisgl_max(scene_max.y - scene_min.y, tmp);
    tmp = aisgl_max(scene_max.z - scene_min.z, tmp);
    tmp = 1.f / tmp;
    glScalef(tmp, tmp, tmp);
    float xc = (scene_min.x + scene_max.x) * 0.5;
    float yc = (scene_min.y + scene_max.y) * 0.5;
    float zc = (scene_min.z + scene_max.z) * 0.5;
    // center the model
    glTranslatef(-xc, -yc, -zc);

    render(scene, scene->mRootNode);

    //~ drawFloor();

    glutSwapBuffers();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(600, 600);
    glutCreateWindow("Model Loader");
    glutInitContextVersion(4, 2);
    glutInitContextProfile(GLUT_CORE_PROFILE);

    initialise();
    glutDisplayFunc(display);
    glutTimerFunc(50, update, 0);
    glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMainLoop();

    aiReleaseImport(scene);
}