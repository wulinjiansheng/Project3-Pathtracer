// CIS565 CUDA Raytracer: A parallel raytracer for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania
// This file includes code from:
//       Yining Karl Li's TAKUA Render, a massively parallel pathtracing renderer: http://www.yiningkarlli.com
// Edited by Liam Boone for use with CUDA v5.5

#include <iostream>
#include "scene.h"
#include <cstring>
//Add FreeImage.lib
#pragma   comment(lib,"FreeImage.lib")
extern bool texturemap_b;
extern bool bumpmap_b;

scene::scene(string filename){
	colors.clear();
	lastnum.clear();
	bump_colors.clear();
	bump_lastnum.clear();
	cout << "Reading scene from " << filename << " ..." << endl;
	cout << " " << endl;
	char* fname = (char*)filename.c_str();
	fp_in.open(fname);
	if(fp_in.is_open()){
		while(fp_in.good()){
			string line;
            utilityCore::safeGetline(fp_in,line);
			if(!line.empty()){
				vector<string> tokens = utilityCore::tokenizeString(line);
				if(strcmp(tokens[0].c_str(), "MATERIAL")==0){
				    loadMaterial(tokens[1]);
				    cout << " " << endl;
				}else if(strcmp(tokens[0].c_str(), "OBJECT")==0){
				    loadObject(tokens[1]);
				    cout << " " << endl;
				}else if(strcmp(tokens[0].c_str(), "CAMERA")==0){
				    loadCamera();
				    cout << " " << endl;
				}
			}
		}
	}
}

bool LoadPic(string tfilename,vector<uint3> &colors,vector<int> &lastnum, int &nHeight,int &nWidth)
{
	FIBITMAP* bitmap;
	bitmap =FreeImage_Load(FreeImage_GetFileType(tfilename.c_str(), 0), tfilename.c_str());
	bitmap = FreeImage_ConvertTo32Bits(bitmap);
	 
	nWidth = FreeImage_GetWidth(bitmap);
	nHeight = FreeImage_GetHeight(bitmap);
	int l = nWidth*nHeight;
	if(lastnum.size()>0)
		l += lastnum[lastnum.size()-1];

	lastnum.push_back(l);
	for(int i=0;i<nWidth;i++)
	{
	   for(int j=0;j<nHeight;j++)
	   {
		   RGBQUAD color;
		   FreeImage_GetPixelColor(bitmap, i, j, &color);
		   uint3 rgb;
		   rgb.x = color.rgbRed;
		   rgb.y = color.rgbGreen;
		   rgb.z = color.rgbBlue;
	       colors.push_back(rgb);
	   }
	}
	
	FreeImage_Unload(bitmap);
	if(nWidth==0&&nHeight==0) return false;
	else return true;
}


int scene::loadObject(string objectid){
    int id = atoi(objectid.c_str());
	std::vector<triangle> tris;
	glm::vec3 maxp,minp;
	//if(id!=objects.size()){
	//	cout << "ERROR: OBJECT ID does not match expected number of objects" << endl;
	//	return -1;
	//}else{
        cout << "Loading Object " << id << "..." << endl;
        geom newObject;
        string line;
        
        //load object type 
        utilityCore::safeGetline(fp_in,line);
        if (!line.empty() && fp_in.good()){
            if(strcmp(line.c_str(), "sphere")==0){
                cout << "Creating new sphere..." << endl;
				newObject.type = SPHERE;
            }else if(strcmp(line.c_str(), "cube")==0){
                cout << "Creating new cube..." << endl;
				newObject.type = CUBE;
            }else{
				string objline = line;
                string name;
                string extension;
                istringstream liness(objline);
                getline(liness, name, '.');
                getline(liness, extension, '.');
                if(strcmp(extension.c_str(), "obj")==0){
                    cout << "Creating new mesh..." << endl;
                    cout << "Reading mesh from " << line << "... " << endl;
					newObject.type = MESH;
					//Add to load obj file
					std::vector<glm::vec3> vv,vi,fn;
					
					OBJreader(vv,fn,vi,objline,maxp,minp);
					for(int i=0;i<(int)vi.size();i++)
					{
					   triangle newtri(vv[vi[i][0]],vv[vi[i][1]],vv[vi[i][2]],fn[i]);
					   tris.push_back(newtri);
					}		
                }else{
                    cout << "ERROR: " << line << " is not a valid object type!" << endl;
                    return -1;
                }
            }
     //}
       
	//link material
    utilityCore::safeGetline(fp_in,line);
	if(!line.empty() && fp_in.good()){
	    vector<string> tokens = utilityCore::tokenizeString(line);
	    newObject.materialid = atoi(tokens[1].c_str());
	    cout << "Connecting Object " << objectid << " to Material " << newObject.materialid << "..." << endl;
    }
        
	//load frames
    int frameCount = 0;
    utilityCore::safeGetline(fp_in,line);
	vector<glm::vec3> translations;
	vector<glm::vec3> scales;
	vector<glm::vec3> rotations;
	vector<glm::vec3>  MBV;
	
    while (!line.empty() && fp_in.good()){
	    
	    //check frame number
	    vector<string> tokens = utilityCore::tokenizeString(line);
        if(strcmp(tokens[0].c_str(), "frame")!=0 || atoi(tokens[1].c_str())!=frameCount){
            cout << "ERROR: Incorrect frame count!" << endl;
            return -1;
        }
	    
	    //load tranformations
	    for(int i=0; i<6; i++){
            glm::vec3 translation; glm::vec3 rotation; glm::vec3 scale;
            utilityCore::safeGetline(fp_in,line);
            tokens = utilityCore::tokenizeString(line);
            if(strcmp(tokens[0].c_str(), "TRANS")==0){
                translations.push_back(glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str())));
            }else if(strcmp(tokens[0].c_str(), "ROTAT")==0){
                rotations.push_back(glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str())));
            }else if(strcmp(tokens[0].c_str(), "SCALE")==0){
                scales.push_back(glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str())));
            }
			else if(strcmp(tokens[0].c_str(), "MBV")==0){
				MBV.push_back(glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str())));
			}
			else if(strcmp(tokens[0].c_str(), "MAP")==0){
				if(texturemap_b)
				{
					string ext = "";
					int h= 0,w=0;
					newObject.theight = h;
					newObject.twidth = w;
					newObject.texindex = -1;
					if(tokens.size()>1&&tokens[1].size()>=4)
						ext = tokens[1].substr(tokens[1].length()-3,tokens[1].length());

					if(strcmp(ext.c_str(), "jpg")==0||strcmp(ext.c_str(), "png")==0||strcmp(ext.c_str(), "bmp")==0)
					{
						if(LoadPic(tokens[1].c_str(),colors,lastnum,h,w))
						{
							cout<<"Finish load texture map"<<lastnum.size()<<endl;
							newObject.texindex = lastnum.size()-1;
							newObject.theight = h;
							newObject.twidth = w;
						}
						else
							cout<<"Texture map doesn't exist"<<endl;
					}	
				}			
			}
			else if(strcmp(tokens[0].c_str(), "BUMP")==0){
				if(bumpmap_b)
				{
					string ext = "";
					int h= 0,w=0;
					newObject.bheight = h;
					newObject.bwidth = w;
					newObject.bumpindex = -1;
					if(tokens.size()>1&&tokens[1].size()>=4)
						ext = tokens[1].substr(tokens[1].length()-3,tokens[1].length());

					if(strcmp(ext.c_str(), "jpg")==0||strcmp(ext.c_str(), "png")==0||strcmp(ext.c_str(), "bmp")==0)
					{
						if(LoadPic(tokens[1].c_str(),bump_colors,bump_lastnum,h,w))
						{
							cout<<"Finish load bump map"<<bump_lastnum.size()<<endl;
							newObject.bumpindex = bump_lastnum.size()-1;
							newObject.bheight = h;
							newObject.bwidth = w;
						}
						else
							cout<<"Bump map doesn't exist"<<endl;
					}	
				}			
			}
	    }
	    
	    frameCount++;
        utilityCore::safeGetline(fp_in,line);
	}
	
	//move frames into CUDA readable arrays
	newObject.translations = new glm::vec3[frameCount];
	newObject.rotations = new glm::vec3[frameCount];
	newObject.scales = new glm::vec3[frameCount];
	newObject.transforms = new cudaMat4[frameCount];
	newObject.inverseTransforms = new cudaMat4[frameCount];
	newObject.transinverseTransforms = new cudaMat4[frameCount];
	newObject.MBV = new glm::vec3[frameCount];
	for(int i=0; i<frameCount; i++){
		newObject.MBV[i] = MBV[i];
		newObject.translations[i] = translations[i];
		newObject.rotations[i] = rotations[i];
		newObject.scales[i] = scales[i];
		glm::mat4 transform = utilityCore::buildTransformationMatrix(translations[i], rotations[i], scales[i]);
		newObject.transforms[i] = utilityCore::glmMat4ToCudaMat4(transform);
		newObject.inverseTransforms[i] = utilityCore::glmMat4ToCudaMat4(glm::inverse(transform));
		newObject.transinverseTransforms[i] = utilityCore::glmMat4ToCudaMat4(glm::transpose(glm::inverse(transform)));
	}
	

	if(newObject.type!=MESH)
        objects.push_back(newObject);
	else
	{
		geom OBJ = newObject;
		for(int i=0;i<(int)tris.size();i++)
		{
			OBJ.tri = tris[i];
			OBJ.trinum = tris.size();
			objects.push_back(OBJ);
		}	
	}
	
	cout << "Loaded " << frameCount << " frames for Object " << objectid << "!" << endl;
        return 1;
    }
}

int scene::loadCamera(){
	cout << "Loading Camera ..." << endl;
        camera newCamera;
	float fovy;
	float focall;
	float blurr;
	//load static properties
	for(int i=0; i<4; i++){
		string line;
        utilityCore::safeGetline(fp_in,line);
		vector<string> tokens = utilityCore::tokenizeString(line);
		if(strcmp(tokens[0].c_str(), "RES")==0){
			newCamera.resolution = glm::vec2(atoi(tokens[1].c_str()), atoi(tokens[2].c_str()));
		}else if(strcmp(tokens[0].c_str(), "FOVY")==0){
			fovy = (float)atof(tokens[1].c_str());
		}else if(strcmp(tokens[0].c_str(), "ITERATIONS")==0){
			newCamera.iterations = atoi(tokens[1].c_str());
		}else if(strcmp(tokens[0].c_str(), "FILE")==0){
			newCamera.imageName = tokens[1];
		}
	}
        
	//load time variable properties (frames)
    int frameCount = 0;
	string line;
    utilityCore::safeGetline(fp_in,line);
	vector<glm::vec3> positions;
	vector<glm::vec3> views;
	vector<glm::vec3> ups;
    while (!line.empty() && fp_in.good()){
	    
	    //check frame number
	    vector<string> tokens = utilityCore::tokenizeString(line);
        if(strcmp(tokens[0].c_str(), "frame")!=0 || atoi(tokens[1].c_str())!=frameCount){
            cout << "ERROR: Incorrect frame count!" << endl;
            return -1;
        }
	    
	    //load camera properties
	    for(int i=0; i<5; i++){
            //glm::vec3 translation; glm::vec3 rotation; glm::vec3 scale;
            utilityCore::safeGetline(fp_in,line);
            tokens = utilityCore::tokenizeString(line);
            if(strcmp(tokens[0].c_str(), "EYE")==0){
                positions.push_back(glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str())));
            }else if(strcmp(tokens[0].c_str(), "VIEW")==0){
                views.push_back(glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str())));
            }else if(strcmp(tokens[0].c_str(), "UP")==0){
                ups.push_back(glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str())));
            }
			else if(strcmp(tokens[0].c_str(), "DOFL")==0){
				focall = atof(tokens[1].c_str());
			}else if(strcmp(tokens[0].c_str(), "DOFR")==0){
				blurr = atof(tokens[1].c_str());
			}
	    }
	    
	    frameCount++;
        utilityCore::safeGetline(fp_in,line);
	}
	newCamera.frames = frameCount;
	
	//move frames into CUDA readable arrays
	newCamera.positions = new glm::vec3[frameCount];
	newCamera.views = new glm::vec3[frameCount];
	newCamera.ups = new glm::vec3[frameCount];
	for(int i = 0; i < frameCount; i++){
		newCamera.positions[i] = positions[i];
		newCamera.views[i] = views[i];
		newCamera.ups[i] = ups[i];
	}

	//calculate fov based on resolution
	float yscaled = tan(fovy*(PI/180));
	float xscaled = (yscaled * newCamera.resolution.x)/newCamera.resolution.y;
	float fovx = (atan(xscaled)*180)/PI;
	newCamera.fov = glm::vec2(fovx, fovy);
	newCamera.blurr = blurr;
	newCamera.focall = focall;

	renderCam = newCamera;
	
	//set up render camera stuff
	renderCam.image = new glm::vec3[(int)renderCam.resolution.x*(int)renderCam.resolution.y];
	renderCam.rayList = new ray[(int)renderCam.resolution.x*(int)renderCam.resolution.y];
	for(int i=0; i<renderCam.resolution.x*renderCam.resolution.y; i++){
		renderCam.image[i] = glm::vec3(0,0,0);
	}
	
	cout << "Loaded " << frameCount << " frames for camera!" << endl;
	return 1;
}

int scene::loadMaterial(string materialid){
	int id = atoi(materialid.c_str());
	if(id!=materials.size()){
		cout << "ERROR: MATERIAL ID does not match expected number of materials" << endl;
		return -1;
	}else{
		cout << "Loading Material " << id << "..." << endl;
		material newMaterial;
	
		//load static properties
		for(int i=0; i<10; i++){
			string line;
            utilityCore::safeGetline(fp_in,line);
			vector<string> tokens = utilityCore::tokenizeString(line);
			if(strcmp(tokens[0].c_str(), "RGB")==0){
				glm::vec3 color( atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()) );
				newMaterial.color = color;
			}else if(strcmp(tokens[0].c_str(), "SPECEX")==0){
				newMaterial.specularExponent = (float)atof(tokens[1].c_str());				  
			}else if(strcmp(tokens[0].c_str(), "SPECRGB")==0){
				glm::vec3 specColor( atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()) );
				newMaterial.specularColor = specColor;
			}else if(strcmp(tokens[0].c_str(), "REFL")==0){
				newMaterial.hasReflective = (float)atof(tokens[1].c_str());
			}else if(strcmp(tokens[0].c_str(), "REFR")==0){
				newMaterial.hasRefractive = (float)atof(tokens[1].c_str());
			}else if(strcmp(tokens[0].c_str(), "REFRIOR")==0){
				newMaterial.indexOfRefraction = (float)atof(tokens[1].c_str());					  
			}else if(strcmp(tokens[0].c_str(), "SCATTER")==0){
				newMaterial.hasScatter = (float)atof(tokens[1].c_str());
			}else if(strcmp(tokens[0].c_str(), "ABSCOEFF")==0){
				glm::vec3 abscoeff( atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()) );
				newMaterial.absorptionCoefficient = abscoeff;
			}else if(strcmp(tokens[0].c_str(), "RSCTCOEFF")==0){
				newMaterial.reducedScatterCoefficient = (float)atof(tokens[1].c_str());					  
			}else if(strcmp(tokens[0].c_str(), "EMITTANCE")==0){
				newMaterial.emittance = (float)atof(tokens[1].c_str());					  
			
			}
		}
		materials.push_back(newMaterial);
		return 1;
	}
}
