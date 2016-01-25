/* \author Geoffrey Biggs */
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include <boost/thread/thread.hpp>
#include <pcl/common/common_headers.h>
#include <pcl/features/normal_3d.h>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/console/parse.h>
#include <pcl/io/ply_io.h>
#include <pcl/io/vtk_lib_io.h>
#include <pcl/point_types.h>
#include <pcl/common/transforms.h>

// Euclidean Cluster Extraction
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/filters/voxel_grid.h>

// region growing segmentation
#include <pcl/filters/passthrough.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/filters/extract_indices.h>

#include <pcl/segmentation/region_growing_rgb.h>
#include <pcl/features/moment_of_inertia_estimation.h>

// tracking
#include <pcl/tracking/tracking.h>
#include <pcl/tracking/particle_filter.h>
#include <pcl/tracking/kld_adaptive_particle_filter_omp.h>
#include <pcl/tracking/particle_filter_omp.h>

#include <pcl/tracking/coherence.h>
#include <pcl/tracking/distance_coherence.h>
#include <pcl/tracking/hsv_color_coherence.h>
#include <pcl/tracking/normal_coherence.h>

#include <pcl/tracking/approx_nearest_pair_point_cloud_coherence.h>
#include <pcl/tracking/nearest_pair_point_cloud_coherence.h>

#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/approximate_voxel_grid.h>
#include <pcl/segmentation/sac_segmentation.h>

#include <pcl/common/time.h>
#include <pcl/common/centroid.h>

#include <boost/format.hpp>

#include "OutputCloud.h"

using namespace std;
using namespace pcl;

//Globals
PointCloud<pcl::PointXYZ>::Ptr cloud_pass_;
PointCloud<pcl::PointXYZ>::Ptr cloud_pass_downsampled_;
PointCloud<pcl::PointXYZ>::Ptr target_cloud;

//boost::mutex mtx_;
typedef boost::shared_ptr<tracking::ParticleFilterTracker<PointXYZ, tracking::ParticleXYZRPY>> ParticleFilter;
list<ParticleFilter> trackerList;

//bool new_cloud_;
double downsampling_grid_size_;
int counter;

//template <typename PointType>
// --------------
// -----Help-----
// --------------
void
printUsage(const char* progName)
{
	std::cout << "\n\nUsage: " << progName << " [options]\n\n"
		<< "Options:\n"
		<< "-------------------------------------------\n"
		<< "-h           this help\n"
		<< "-s           Simple visualisation example\n"
		<< "-r           RGB colour visualisation example\n"
		<< "-c           Custom colour visualisation example\n"
		<< "-n           Normals visualisation example\n"
		<< "-a           Shapes visualisation example\n"
		<< "-v           Viewports example\n"
		<< "-i           Interaction Customization example\n"
		<< "\n\n";
}


boost::shared_ptr<pcl::visualization::PCLVisualizer> simpleVis(pcl::PointCloud<pcl::PointXYZ>::ConstPtr cloud)
{
	// --------------------------------------------
	// -----Open 3D viewer and add point cloud-----
	// --------------------------------------------
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->setBackgroundColor(0, 0, 0);
	viewer->addPointCloud<pcl::PointXYZ> (cloud, "sample cloud");
	viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud");
	viewer->addCoordinateSystem(1.0);
	viewer->initCameraParameters();
	return (viewer);
}


boost::shared_ptr<pcl::visualization::PCLVisualizer> rgbVis(pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr cloud)
{
	// --------------------------------------------
	// -----Open 3D viewer and add point cloud-----
	// --------------------------------------------
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->setBackgroundColor(0, 0, 0);
	pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(cloud);
	viewer->addPointCloud<pcl::PointXYZRGB>(cloud, rgb, "sample cloud");
	viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
	viewer->addCoordinateSystem(1.0);
	viewer->initCameraParameters();
	return (viewer);
}


boost::shared_ptr<pcl::visualization::PCLVisualizer> customColourVis(pcl::PointCloud<pcl::PointXYZ>::ConstPtr cloud)
{
	// --------------------------------------------
	// -----Open 3D viewer and add point cloud-----
	// --------------------------------------------
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->setBackgroundColor(0, 0, 0);
	pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> single_color(cloud, 0, 255, 0);
	viewer->addPointCloud<pcl::PointXYZ>(cloud, single_color, "sample cloud");
	viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
	viewer->addCoordinateSystem(1.0);
	viewer->initCameraParameters();
	return (viewer);
}


boost::shared_ptr<pcl::visualization::PCLVisualizer> normalsVis(
	pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr cloud, pcl::PointCloud<pcl::Normal>::ConstPtr normals)
{
	// --------------------------------------------------------
	// -----Open 3D viewer and add point cloud and normals-----
	// --------------------------------------------------------
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->setBackgroundColor(0, 0, 0);
	pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(cloud);
	viewer->addPointCloud<pcl::PointXYZRGB>(cloud, rgb, "sample cloud");
	viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
	viewer->addPointCloudNormals<pcl::PointXYZRGB, pcl::Normal>(cloud, normals, 10, 0.05, "normals");
	viewer->addCoordinateSystem(1.0);
	viewer->initCameraParameters();
	return (viewer);
}


boost::shared_ptr<pcl::visualization::PCLVisualizer> shapesVis(pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr cloud)
{
	// --------------------------------------------
	// -----Open 3D viewer and add point cloud-----
	// --------------------------------------------
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->setBackgroundColor(0, 0, 0);
	pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(cloud);
	viewer->addPointCloud<pcl::PointXYZRGB>(cloud, rgb, "sample cloud");
	viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
	viewer->addCoordinateSystem(1.0);
	viewer->initCameraParameters();

	//------------------------------------
	//-----Add shapes at cloud points-----
	//------------------------------------
	viewer->addLine<pcl::PointXYZRGB>(cloud->points[0],
		cloud->points[cloud->size() - 1], "line");
	viewer->addSphere(cloud->points[0], 0.2, 0.5, 0.5, 0.0, "sphere");

	//---------------------------------------
	//-----Add shapes at other locations-----
	//---------------------------------------
	pcl::ModelCoefficients coeffs;
	coeffs.values.push_back(0.0);
	coeffs.values.push_back(0.0);
	coeffs.values.push_back(1.0);
	coeffs.values.push_back(0.0);
	viewer->addPlane(coeffs, "plane");
	coeffs.values.clear();
	coeffs.values.push_back(0.3);
	coeffs.values.push_back(0.3);
	coeffs.values.push_back(0.0);
	coeffs.values.push_back(0.0);
	coeffs.values.push_back(1.0);
	coeffs.values.push_back(0.0);
	coeffs.values.push_back(5.0);
	viewer->addCone(coeffs, "cone");

	return (viewer);
}


boost::shared_ptr<pcl::visualization::PCLVisualizer> viewportsVis(
	pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr cloud, pcl::PointCloud<pcl::Normal>::ConstPtr normals1, pcl::PointCloud<pcl::Normal>::ConstPtr normals2)
{
	// --------------------------------------------------------
	// -----Open 3D viewer and add point cloud and normals-----
	// --------------------------------------------------------
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->initCameraParameters();

	int v1(0);
	viewer->createViewPort(0.0, 0.0, 0.5, 1.0, v1);
	viewer->setBackgroundColor(0, 0, 0, v1);
	viewer->addText("Radius: 0.01", 10, 10, "v1 text", v1);
	pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(cloud);
	viewer->addPointCloud<pcl::PointXYZRGB>(cloud, rgb, "sample cloud1", v1);

	int v2(0);
	viewer->createViewPort(0.5, 0.0, 1.0, 1.0, v2);
	viewer->setBackgroundColor(0.3, 0.3, 0.3, v2);
	viewer->addText("Radius: 0.1", 10, 10, "v2 text", v2);
	pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZRGB> single_color(cloud, 0, 255, 0);
	viewer->addPointCloud<pcl::PointXYZRGB>(cloud, single_color, "sample cloud2", v2);

	viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud1");
	viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud2");
	viewer->addCoordinateSystem(1.0);

	viewer->addPointCloudNormals<pcl::PointXYZRGB, pcl::Normal>(cloud, normals1, 10, 0.05, "normals1", v1);
	viewer->addPointCloudNormals<pcl::PointXYZRGB, pcl::Normal>(cloud, normals2, 10, 0.05, "normals2", v2);

	return (viewer);
}


unsigned int text_id = 0;
void keyboardEventOccurred(const pcl::visualization::KeyboardEvent &event,
	void* viewer_void)
{
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = *static_cast<boost::shared_ptr<pcl::visualization::PCLVisualizer> *> (viewer_void);
	if (event.getKeySym() == "r" && event.keyDown())
	{
		std::cout << "r was pressed => removing all text" << std::endl;

		char str[512];
		for (unsigned int i = 0; i < text_id; ++i)
		{
			sprintf(str, "text#%03d", i);
			viewer->removeShape(str);
		}
		text_id = 0;
	} 
	else if (event.getKeySym() == "w" && event.keyDown())
	{
		std::cout << "w was pressed => moving closer" << std::endl;

		//viewer->setCameraPosition(cam[0].pos[0] + 0.2, cam[0].pos[1], cam[0].pos[2], cam[0].view[0], cam[0].view[1], cam[0].view[2], 0);
	}
}

void mouseEventOccurred(const pcl::visualization::MouseEvent &event,
	void* viewer_void)
{
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = *static_cast<boost::shared_ptr<pcl::visualization::PCLVisualizer> *> (viewer_void);
	if (event.getButton() == pcl::visualization::MouseEvent::LeftButton &&
		event.getType() == pcl::visualization::MouseEvent::MouseButtonRelease)
	{
		
		std::cout << "Left mouse button released at position (" << event.getX() << ", " << event.getY() << ")" << std::endl;

		char str[512];
		sprintf(str, "text#%03d", text_id++);
		viewer->addText("clicked here", event.getX(), event.getY(), str);
	}
}

boost::shared_ptr<pcl::visualization::PCLVisualizer> interactionCustomizationVis()
{
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->setBackgroundColor(0, 0, 0);
	viewer->addCoordinateSystem(1.0);

	viewer->registerKeyboardCallback(keyboardEventOccurred, (void*)&viewer);
	viewer->registerMouseCallback(mouseEventOccurred, (void*)&viewer);

	return (viewer);
}

void visualizePointCloudClusters(std::vector<pcl::PointIndices> cluster_indices, pcl::PointCloud<pcl::PointXYZ>::Ptr cloudXYZ){

	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->setBackgroundColor(0, 0, 0);
	int j = 0;
	for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); ++it)
	{
		pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster(new pcl::PointCloud<pcl::PointXYZ>);
		for (std::vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); ++pit){
			cloud_cluster->points.push_back(cloudXYZ->points[*pit]); 
		}
		
		cloud_cluster->width = cloud_cluster->points.size();
		cloud_cluster->height = 1;
		cloud_cluster->is_dense = true;

		cout << "cluster " << j << endl;

		// calculate bounding box
		pcl::MomentOfInertiaEstimation <pcl::PointXYZ> feature_extractor;
		feature_extractor.setInputCloud(cloud_cluster);
		feature_extractor.compute();

		std::vector <float> moment_of_inertia;
		std::vector <float> eccentricity;
		pcl::PointXYZ min_point_AABB;
		pcl::PointXYZ max_point_AABB;

		feature_extractor.getMomentOfInertia(moment_of_inertia);
		feature_extractor.getEccentricity(eccentricity);
		feature_extractor.getAABB(min_point_AABB, max_point_AABB);



		// visualize clusters
		pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> single_color(cloud_cluster, (j * 3 * 10), 80, j * 3 * 20);
		viewer->addPointCloud<pcl::PointXYZ>(cloud_cluster, single_color, "sample cloud" + j);
		viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud" + j); (cloud_cluster);
		viewer->addCube(min_point_AABB.x, max_point_AABB.x, min_point_AABB.y, max_point_AABB.y, min_point_AABB.z, max_point_AABB.z, 1.0, 1.0, 0.0, "AABB" + j);

		std::cout << "AABB" << j << std::endl;
		std::cout << "cloud_cluster_" << j << "\t" << "color  = " << (j * 10) << "," << 255 << "," << j * 20 << std::endl;

		j++;
	}

	viewer->addCoordinateSystem(1.0);
	viewer->initCameraParameters();

	//viewer->registerKeyboardCallback(keyboardEventOccurred, (void*)&viewer);
	//viewer->registerMouseCallback(mouseEventOccurred, (void*)&viewer);


	// visualize result
	while (!viewer->wasStopped())
	{
		viewer->spinOnce(100);
		boost::this_thread::sleep(boost::posix_time::microseconds(100000));
	}

	// closes the window after pressing 'q'
	viewer->close();
	
	return;
}

void visualizePointCloud(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud, int clusterID){

	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("Viewer 3D"));
	viewer->setBackgroundColor(0, 0, 0);
	viewer->addCoordinateSystem(1.0);
	char str[512];
	sprintf(str, "CLUSTER%i", clusterID);
	viewer->addText(str, 100, 100);

	//viewer->addText(clusterID, 0, 0, clusterID);
	
	// calculate bounding box
	PointCloud<PointXYZ>::Ptr cloudXYZ(new PointCloud<PointXYZ>);
	copyPointCloud<PointXYZRGB, PointXYZ>(*cloud, *cloudXYZ);
	pcl::MomentOfInertiaEstimation <pcl::PointXYZ> feature_extractor;
	feature_extractor.setInputCloud(cloudXYZ);
	feature_extractor.compute();

	std::vector <float> moment_of_inertia;
	std::vector <float> eccentricity;
	pcl::PointXYZ min_point_AABB;
	pcl::PointXYZ max_point_AABB;

	feature_extractor.getMomentOfInertia(moment_of_inertia);
	feature_extractor.getEccentricity(eccentricity);
	feature_extractor.getAABB(min_point_AABB, max_point_AABB);



	// visualize clusters
	//pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> single_color(cloudXYZ, 30, 80, 3 * 20);
	//viewer->addPointCloud<pcl::PointXYZ>(cloudXYZ, single_color, "sample cloud");
	pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(cloud);
	viewer->addPointCloud<pcl::PointXYZRGB>(cloud, rgb, "sample cloud");
	//viewer->addPointCloud(cloud);
	viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud"); (cloud);
	viewer->addCube(min_point_AABB.x, max_point_AABB.x, min_point_AABB.y, max_point_AABB.y, min_point_AABB.z, max_point_AABB.z, 1.0, 1.0, 0.0, "AABB");

	// visualize result
	while (!viewer->wasStopped())
	{
		viewer->spinOnce(100);
		boost::this_thread::sleep(boost::posix_time::microseconds(100000));
	}

	// closes the window after pressing 'q'
	viewer->close();

	return;
}

void pclVisualizer(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud){

	bool simple(false), rgb(false), custom_c(false), normals(false),
		shapes(false), viewports(false), interaction_customization(false);
	
	// ------------------------------------
	// -----Create example point cloud-----
	// ------------------------------------
	pcl::PointCloud<pcl::PointXYZ>::Ptr basic_cloud_ptr(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr point_cloud_ptr(new pcl::PointCloud<pcl::PointXYZRGB>);
	std::cout << "Genarating example point clouds.\n\n";
	// We're going to make an ellipse extruded along the z-axis. The colour for
	// the XYZRGB cloud will gradually go from red to green to blue.
	uint8_t r(255), g(15), b(15);
	for (float z(-1.0); z <= 1.0; z += 0.05)
	{
	for (float angle(0.0); angle <= 360.0; angle += 5.0)
	{
	pcl::PointXYZ basic_point;
	basic_point.x = 0.5 * cosf(pcl::deg2rad(angle));
	basic_point.y = sinf(pcl::deg2rad(angle));
	basic_point.z = z;
	basic_cloud_ptr->points.push_back(basic_point);

	pcl::PointXYZRGB point;
	point.x = basic_point.x;
	point.y = basic_point.y;
	point.z = basic_point.z;
	uint32_t rgb = (static_cast<uint32_t>(r) << 16 |
	static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(b));
	point.rgb = *reinterpret_cast<float*>(&rgb);
	point_cloud_ptr->points.push_back(point);
	}
	if (z < 0.0)
	{
	r -= 12;
	g += 12;
	}
	else
	{
	g -= 12;
	b += 12;
	}
	}
	basic_cloud_ptr->width = (int)basic_cloud_ptr->points.size();
	basic_cloud_ptr->height = 1;
	point_cloud_ptr->width = (int)point_cloud_ptr->points.size();
	point_cloud_ptr->height = 1;


	// LOAD STANDFORD BUNNY
	pcl::PolygonMesh mesh;
	pcl::io::loadPolygonFilePLY("D:\\PointCloudSegmentation\\bun_love.ply", mesh);
	fromPCLPointCloud2(mesh.cloud, *point_cloud_ptr);

	// ----------------------------------------------------------------
	// -----Calculate surface normals with a search radius of 0.05-----
	// ----------------------------------------------------------------
	pcl::NormalEstimation<pcl::PointXYZRGB, pcl::Normal> ne;
	ne.setInputCloud(point_cloud_ptr);
	pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZRGB>());
	ne.setSearchMethod(tree);
	pcl::PointCloud<pcl::Normal>::Ptr cloud_normals1(new pcl::PointCloud<pcl::Normal>);
	ne.setRadiusSearch(0.0005);
	ne.compute(*cloud_normals1);

	// ---------------------------------------------------------------
	// -----Calculate surface normals with a search radius of 0.1-----
	// ---------------------------------------------------------------
	pcl::PointCloud<pcl::Normal>::Ptr cloud_normals2(new pcl::PointCloud<pcl::Normal>);
	ne.setRadiusSearch(0.1);
	ne.compute(*cloud_normals2);
	
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;
	if (simple)
	{
		viewer = simpleVis(basic_cloud_ptr);
	}
	else if (rgb)
	{
		viewer = rgbVis(point_cloud_ptr);
	}
	else if (custom_c)
	{
		viewer = customColourVis(basic_cloud_ptr);
	}
	else if (normals)
	{
		viewer = normalsVis(point_cloud_ptr, cloud_normals1);
	}
	else if (shapes)
	{
		viewer = shapesVis(point_cloud_ptr);
	}
	else if (viewports)
	{
		//	viewer = viewportsVis(point_cloud_ptr, cloud_normals1, cloud_normals2);
	}
	else if (interaction_customization)
	{
		viewer = interactionCustomizationVis();
	}

	//--------------------
	// -----Main loop-----
	//--------------------
	while (!viewer->wasStopped())
	{
		viewer->spinOnce(100);
		boost::this_thread::sleep(boost::posix_time::microseconds(100000));
	}

}

// ----------------------------------------------------------------------
// -----LOAD POINT CLOUD DATA--------------------------------------------
// ----------------------------------------------------------------------

vector<string> getFilesInDirectory(const string directory)
{
	HANDLE dir;
	WIN32_FIND_DATA file_data;
	vector<string> out;
	string direc = directory + "/*";
	const char* temp = direc.c_str();

	if ((dir = FindFirstFile(temp, &file_data)) == INVALID_HANDLE_VALUE)
		return out;

	do {
		const string file_name = file_data.cFileName;
		const string full_file_name = directory + "\\" + file_name;
		const bool is_directory = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

		if (file_name[0] == '.')
			continue;

		if (is_directory)
			continue;

		out.push_back(full_file_name);
	} while (FindNextFile(dir, &file_data));

	FindClose(dir);
	return out;
}

vector<string> sortFilenames(vector<string> filenames){

	vector<int> temp = vector<int>(sizeof(filenames));
	vector<string> sortedFilenames = vector<string>(sizeof(filenames));
	string path = "";
	string perfix = "outputCloud";

	temp.clear();
	vector<string>::const_iterator iter;
	for (iter = filenames.begin(); iter != filenames.end(); ++iter)
	{
		int indexSeparator = (*iter).find_last_of('\\');
		path = (*iter).substr(0, indexSeparator);

		string name = (*iter).substr(indexSeparator + 1, (*iter).length());

		int cloudNumber = atoi(name.substr(perfix.length(), name.length()).c_str());
		temp.push_back(cloudNumber);

	}

	sort(temp.begin(), temp.end());

	vector<int>::const_iterator iter_int;
	for (iter_int = temp.begin(); iter_int != temp.end(); ++iter_int){
		string name = path + '\\' + perfix + boost::lexical_cast<string>(*iter_int);
		sortedFilenames.push_back(name);
		cout << name << endl;
	}

	return sortedFilenames;
}

/*void loadPointCloudNoFormat(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud, string pointCloudPath){
	std::ifstream f;
	f.open(pointCloudPath, fstream::in);
	std::string line;
	string delimiter1 = " ";
	
	string *pos = new string[6];
	while (std::getline(f, line))
	{

		size_t p;
		int i = 0;
		string substring = "";
		while ((p = line.find(delimiter1)) != std::string::npos) {
			substring = pos[i] = line.substr(0, p);
			line.erase(0, p + delimiter1.length());
			i++;
		}
		pos[5] = line;

		pcl::PointXYZRGB point;
		point.x = stof(pos[0]);
		point.y = stof(pos[1]);
		point.z = stof(pos[2]);
		point.r = (uint8_t)stoi(pos[3]);
		point.g = (uint8_t)stoi(pos[4]);
		point.b = (uint8_t)stoi(pos[5]);

		if (point.x != 0 && point.y != 0 && point.z != 0)
			cloud->points.push_back(point);
	}
}

void loadPointCloudsNoFormat(){

	string pointCloudPath = "D:\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\girafa\\Output2\\sample\\";
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr point_cloud_ptr(new pcl::PointCloud<pcl::PointXYZRGB>);


	vector<string> pointCloudFilenames = getFilesInDirectory(pointCloudPath);

	int i = 0;
	vector<string>::const_iterator iter;
	for (iter = pointCloudFilenames.begin(); iter != pointCloudFilenames.end(); ++iter)
	{
		cout << i << endl;
		loadPointCloudNoFormat(point_cloud_ptr, *iter);
		i++;
	}
	cout << "FINISHED LOADING POINT CLOUD DATA" << endl;
	return;
}*/

// ----------------------------------------------------------------------
// -----LOAD POINT TOOLS ------------------------------------------------
// ----------------------------------------------------------------------

//pcl::PointCloud<pcl::PointXYZRGB>::Ptr ApplyCalibrationToPointCloud(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud, string calibrarionFilePath){
//
//	pcl::PointCloud<pcl::PointXYZRGB>::Ptr transformed_cloud(new pcl::PointCloud<pcl::PointXYZRGB>());
//	boost::filesystem::path full_path(boost::filesystem::current_path());
//	
//	// Get calibration data
//	std::ifstream f;
//	f.open(full_path.string() + "\\" + calibrarionFilePath, fstream::in);
//	std::string line;
//	string delimiter = "=";
//	float *translationRotation = new float[6]; // posx, posy, posz, rotx, roty, rotz
//	size_t p;
//	int i = 0;
//	//string substring = "";
//
//	while (std::getline(f, line))
//	{
//		if ((p = line.find(delimiter)) != std::string::npos) {
//			translationRotation[i] = stof(line.substr(p + 1, line.length()));
//			i++;
//		}
//		//pos[5] = line.substr(p+1, line.length());
//	}
//
//	// compute point cloud position
//	// Object to store the centroid coordinates.
//	Eigen::Vector4f centroid;
//
//	pcl::compute3DCentroid(*cloud, centroid);
//
//	std::cout << "The XYZ coordinates of the centroid are: ("
//		<< centroid[0] << ", "
//		<< centroid[1] << ", "
//		<< centroid[2] << ")." << std::endl;
//
//	cout << "translation x = " << translationRotation[0] << " y = " << translationRotation[1] << " z = " << translationRotation[2] << endl;
//	cout << "rotation x = " << translationRotation[3] << " y = " << translationRotation[4] << " z = " << translationRotation[5] << endl;
//
//	
//	Eigen::Affine3f transform_2 = Eigen::Affine3f::Identity();
//
//	// translate to 0
//	// then rotate 
//	// then translate to initial position
//
//	// Define a translation to 0,0,0.
//	//transform_2.translation() << translationRotation[0], translationRotation[1], translationRotation[2];
//	transform_2.translation() << -centroid[0], -centroid[1], -centroid[2];
//
//	/*float thetaX = (M_PI / 180) * translationRotation[3]; // The angle of rotation in radians
//	float thetaY = (M_PI / 180) * translationRotation[4]; // The angle of rotation in radians
//	float thetaZ = (M_PI / 180) * translationRotation[5]; // The angle of rotation in radians */
//
//	float thetaX = deg2rad(translationRotation[3]); // The angle of rotation in radians
//	float thetaY = deg2rad(translationRotation[4]);; // The angle of rotation in radians
//	float thetaZ = deg2rad(translationRotation[5]);; // The angle of rotation in radians 
//	// The  rotation matrix; tetha radians arround X, Y and Z axis
//	transform_2.rotate(Eigen::AngleAxisf(thetaX, Eigen::Vector3f::UnitX()));
//	transform_2.rotate(Eigen::AngleAxisf(thetaY, Eigen::Vector3f::UnitY()));
//	transform_2.rotate(Eigen::AngleAxisf(thetaZ, Eigen::Vector3f::UnitZ()));
//
//	cout << "rotation in radians x = " << thetaX << " y = " << thetaY << " z = " << thetaZ << endl;
//
//	transform_2.translation() << centroid[0], centroid[1], centroid[2];
//
//	transform_2.translation() << translationRotation[0], translationRotation[1], translationRotation[2]; \
//
//	// Apply translate to origin, rotation, translate no original position, translate to calibration position
//	pcl::transformPointCloud(*cloud, *transformed_cloud, transform_2);
//
//	// Print the transformation
//	printf("\nMethod #2: using an Affine3f\n");
//	std::cout << transform_2.matrix() << std::endl;
//
//	return transformed_cloud;
//}




// ----------------------------------------------------------------------
// -----POINT CLOUD SEGMENTATION-----------------------------------------
// ----------------------------------------------------------------------
pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
std::vector<pcl::PointIndices> EuclideanClusterExtractionSegmentation(pcl::PointCloud<pcl::PointXYZ>::Ptr cloudXYZ){

	// Creating the KdTree object for the search method of the extraction
	pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
	tree->setInputCloud(cloudXYZ);

	std::vector<pcl::PointIndices> cluster_indices;
	ec.setClusterTolerance(0.028); // 2,8cm
	ec.setMinClusterSize(2000);
	//ec.setMaxClusterSize(40000);
	ec.setSearchMethod(tree);
	ec.setInputCloud(cloudXYZ);
	ec.extract(cluster_indices);

	return cluster_indices;
}

void RegionGrowingSegmentation(pcl::PointCloud<pcl::PointXYZRGB>::Ptr point_cloud_ptr){

	//copy point cloud no format (Joao Fiadeiro study) XYZRGB to XYZ 
	pcl::PointCloud <pcl::PointXYZ>::Ptr cloudXYZ (new pcl::PointCloud <pcl::PointXYZ>);
	copyPointCloud<PointXYZRGB, PointXYZ>(*point_cloud_ptr, *cloudXYZ);

	pcl::search::Search<pcl::PointXYZ>::Ptr tree = boost::shared_ptr<pcl::search::Search<pcl::PointXYZ> >(new pcl::search::KdTree<pcl::PointXYZ>);
	pcl::PointCloud <pcl::Normal>::Ptr normals(new pcl::PointCloud <pcl::Normal>);
	pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimator;
	normal_estimator.setSearchMethod(tree);
	normal_estimator.setInputCloud(cloudXYZ);
	normal_estimator.setKSearch(50);
	normal_estimator.compute(*normals);

	/*pcl::IndicesPtr indices(new std::vector <int>);
	pcl::PassThrough<pcl::PointXYZRGB> pass;
	pass.setInputCloud(point_cloud_ptr);
	pass.setFilterFieldName("z");
	pass.setFilterLimits(0.0, 1.0);
	pass.filter(*indices);*/

	pcl::RegionGrowing<pcl::PointXYZ, pcl::Normal> reg;
	reg.setMinClusterSize(1000);
//	reg.setMaxClusterSize(1000000);
	reg.setSearchMethod(tree);
	reg.setNumberOfNeighbours(400);
	reg.setInputCloud(cloudXYZ);
	//reg.setIndices (indices);
	reg.setInputNormals(normals);
	reg.setSmoothnessThreshold(3.0 / 180.0 * M_PI);
	reg.setCurvatureThreshold(1.0);

	std::vector <pcl::PointIndices> clusters;
	reg.extract(clusters);

	std::cout << "Number of clusters is equal to " << clusters.size() << std::endl;
	std::cout << "First cluster has " << clusters[0].indices.size() << " points." << endl;
	std::cout << "These are the indices of the points of the initial" <<
		std::endl << "cloud that belong to the first cluster:" << std::endl;
	int counter = 0;
	while (counter < clusters[0].indices.size())
	{
		std::cout << clusters[0].indices[counter] << ", ";
		counter++;
		if (counter % 10 == 0)
			std::cout << std::endl;
	}
	std::cout << std::endl;

	pcl::PointCloud <pcl::PointXYZRGB>::Ptr colored_cloud = reg.getColoredCloud();
	//boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;
	//viewer = rgbVis(point_cloud_ptr);
	pcl::visualization::CloudViewer viewer("Cluster viewer");
	viewer.showCloud(colored_cloud);
	while (!viewer.wasStopped())
	{
	//	viewer->spinOnce(100);
	//	boost::this_thread::sleep(boost::posix_time::microseconds(100000));
	}

	return;

}

void colorBasedRegionGrowingSegmentation(pcl::PointCloud<pcl::PointXYZRGB>::Ptr point_cloud_ptr){

	pcl::search::Search <pcl::PointXYZRGB>::Ptr tree = boost::shared_ptr<pcl::search::Search<pcl::PointXYZRGB> >(new pcl::search::KdTree<pcl::PointXYZRGB>);

	pcl::IndicesPtr indices(new std::vector <int>);
	pcl::PassThrough<pcl::PointXYZRGB> pass;
	pass.setInputCloud(point_cloud_ptr);
	pass.setFilterFieldName("z");
	pass.setFilterLimits(0.0, 1.0);
	pass.filter(*indices);

	pcl::RegionGrowingRGB<pcl::PointXYZRGB> reg;
	reg.setInputCloud(point_cloud_ptr);
	reg.setIndices(indices);
	reg.setSearchMethod(tree);
	reg.setDistanceThreshold(5);
	reg.setPointColorThreshold(10);
	reg.setRegionColorThreshold(5);
	reg.setMinClusterSize(100);

	std::vector <pcl::PointIndices> clusters;
	reg.extract(clusters);

	pcl::PointCloud <pcl::PointXYZRGB>::Ptr colored_cloud = reg.getColoredCloud();
	pcl::visualization::CloudViewer viewer("Cluster viewer");
	viewer.showCloud(colored_cloud);
	while (!viewer.wasStopped())
	{
		boost::this_thread::sleep(boost::posix_time::microseconds(100));
	}

}

// ----------------------------------------------------------------------
// -----POINT CLOUD TRACKING --------------------------------------------
// ----------------------------------------------------------------------
//Filter along a specified dimension
void filterPassThrough(const pcl::PointCloud<pcl::PointXYZ>::Ptr &cloud, pcl::PointCloud<pcl::PointXYZ> &result)
{
	pcl::PassThrough<pcl::PointXYZ> pass;
	pass.setFilterFieldName("z");
	pass.setFilterLimits(0.0, 10.0);
	pass.setKeepOrganized(false);
	pass.setInputCloud(cloud);
	pass.filter(result);
}


void gridSampleApprox(const pcl::PointCloud<pcl::PointXYZ>::Ptr &cloud, pcl::PointCloud<pcl::PointXYZ> &result, double leaf_size)
{
	pcl::ApproximateVoxelGrid<pcl::PointXYZ> grid;
	grid.setLeafSize(static_cast<float> (leaf_size), static_cast<float> (leaf_size), static_cast<float> (leaf_size));
	grid.setInputCloud(cloud);
	grid.filter(result);
}


////Draw the current particles
//bool
//drawParticles(pcl::visualization::PCLVisualizer& viz)
//{
//	pcl::tracking::ParticleFilterTracker<pcl::PointXYZ, pcl::tracking::ParticleXYZRPY>::PointCloudStatePtr particles = tracker_->getParticles();
//	if (particles && new_cloud_)
//	{
//		//Set pointCloud with particle's points
//		pcl::PointCloud<pcl::PointXYZ>::Ptr particle_cloud(new pcl::PointCloud<pcl::PointXYZ>());
//		for (size_t i = 0; i < particles->points.size(); i++)
//		{
//			pcl::PointXYZ point;
//
//			point.x = particles->points[i].x;
//			point.y = particles->points[i].y;
//			point.z = particles->points[i].z;
//			particle_cloud->points.push_back(point);
//		}
//
//		//Draw red particles 
//	  {
//		  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> red_color(particle_cloud, 250, 99, 71);
//
//		  if (!viz.updatePointCloud(particle_cloud, red_color, "particle cloud"))
//			  viz.addPointCloud(particle_cloud, red_color, "particle cloud");
//	  }
//		return true;
//	}
//	else
//	{
//		return false;
//	}
//}
//


//Draw model reference point cloud
/*void
drawTrackingResults()
{
	list<ParticleFilter>::iterator it_trackerList = trackerList.begin();
	int cluster_index = 0;
	while (it_trackerList != trackerList.end()) {
		ParticleFilter particleFilterTemp = *it_trackerList;
		pcl::tracking::ParticleXYZRPY result = particleFilterTemp->getResult();
		Eigen::Affine3f transformation = particleFilterTemp->toEigenMatrix(result);

		//move close to camera a little for better visualization
		transformation.translation() += Eigen::Vector3f(0.0f, 0.0f, -0.005f);
		PointCloud<PointXYZ>::Ptr result_cloud(new PointCloud<PointXYZ>());
		pcl::transformPointCloud<PointXYZ>(*(particleFilterTemp->getReferenceCloud()), *result_cloud, transformation);

		//Draw blue model reference point cloud
		{
			pcl::visualization::PointCloudColorHandlerCustom<PointXYZ> blue_color(result_cloud, 0, 0, 255);
			visualizePointCloud(result_cloud, cluster_index);
			//if (!viz.updatePointCloud(result_cloud, blue_color, "cluster " + cluster_index))
			//viz.addPointCloud(result_cloud, blue_color, "cluster " + cluster_index);
		}
		cout << "cluster " << cluster_index << endl;
		cluster_index++;
		it_trackerList++;
	}
}*/


void setupModelTracking(PointCloud<PointXYZ>::Ptr cloud_cluster){

	//Set parameters
	//new_cloud_ = false;
	downsampling_grid_size_ = 0.02;

	std::vector<double> default_step_covariance = std::vector<double>(6, 0.015 * 0.015);
	default_step_covariance[3] *= 40.0;
	default_step_covariance[4] *= 40.0;
	default_step_covariance[5] *= 40.0;

	std::vector<double> initial_noise_covariance = std::vector<double>(6, 0.00001);
	std::vector<double> default_initial_mean = std::vector<double>(6, 0.0);

	ParticleFilter tracker_;
	boost::shared_ptr<pcl::tracking::KLDAdaptiveParticleFilterOMPTracker<PointXYZ, pcl::tracking::ParticleXYZRPY> > tracker
	(new pcl::tracking::KLDAdaptiveParticleFilterOMPTracker<PointXYZ, pcl::tracking::ParticleXYZRPY>(8));

	pcl::tracking::ParticleXYZRPY bin_size;
	bin_size.x = 0.1f;
	bin_size.y = 0.1f;
	bin_size.z = 0.1f;
	bin_size.roll = 0.1f;
	bin_size.pitch = 0.1f;
	bin_size.yaw = 0.1f;


	//Set all parameters for  KLDAdaptiveParticleFilterOMPTracker
	tracker->setMaximumParticleNum(1000);
	tracker->setDelta(0.99);
	tracker->setEpsilon(0.2);
	tracker->setBinSize(bin_size);

	//Set all parameters for  ParticleFilter
	tracker_ = tracker;
	tracker_->setTrans(Eigen::Affine3f::Identity());
	tracker_->setStepNoiseCovariance(default_step_covariance);
	tracker_->setInitialNoiseCovariance(initial_noise_covariance);
	tracker_->setInitialNoiseMean(default_initial_mean);
	tracker_->setIterationNum(1);
	tracker_->setParticleNum(600);
	tracker_->setResampleLikelihoodThr(0.00);
	tracker_->setUseNormal(false);

	//Setup coherence object for tracking
	pcl::tracking::ApproxNearestPairPointCloudCoherence<PointXYZ>::Ptr coherence = pcl::tracking::ApproxNearestPairPointCloudCoherence<PointXYZ>::Ptr
	(new pcl::tracking::ApproxNearestPairPointCloudCoherence<PointXYZ>());

	boost::shared_ptr<pcl::tracking::DistanceCoherence<PointXYZ> > distance_coherence
	= boost::shared_ptr<pcl::tracking::DistanceCoherence<PointXYZ> >(new pcl::tracking::DistanceCoherence<PointXYZ>());
	coherence->addPointCoherence(distance_coherence);

	boost::shared_ptr<pcl::search::Octree<PointXYZ> > search(new pcl::search::Octree<PointXYZ>(0.01));
	coherence->setSearchMethod(search);
	coherence->setMaximumDistance(0.01);

	tracker_->setCloudCoherence(coherence);
	
	//prepare the model of tracker's target
	Eigen::Vector4f c;
	Eigen::Affine3f trans = Eigen::Affine3f::Identity();
	PointCloud<PointXYZ>::Ptr transed_ref(new PointCloud<PointXYZ>);
	PointCloud<PointXYZ>::Ptr transed_ref_downsampled(new PointCloud<PointXYZ>);

	pcl::compute3DCentroid<PointXYZ>(*cloud_cluster, c);
	trans.translation().matrix() = Eigen::Vector3f(c[0], c[1], c[2]);
	pcl::transformPointCloud<PointXYZ>(*cloud_cluster, *transed_ref, trans.inverse());
	gridSampleApprox(transed_ref, *transed_ref_downsampled, downsampling_grid_size_);

	//set reference model and trans
	tracker_->setReferenceCloud(transed_ref_downsampled);
	tracker_->setTrans(trans);

	trackerList.push_back(tracker_);
}

void
cloud_cb(const PointCloud<PointXYZ>::Ptr &cloud)
{
	//boost::mutex::scoped_lock lock(mtx_);
	//cloud_pass_.reset(new PointCloud<PointXYZ>);
	cloud_pass_downsampled_.reset(new PointCloud<PointXYZ>);
	//filterPassThrough(cloud, *cloud_pass_);

	//visualizePointCloud(cloud_pass_);

	gridSampleApprox(cloud, *cloud_pass_downsampled_, downsampling_grid_size_);

	// DEBUG
	//visualizePointCloud(cloud_pass_downsampled_, "point cloud downsampled");

	list<ParticleFilter>::iterator it_trackerList = trackerList.begin();
	while (it_trackerList != trackerList.end()) {
		//Track the object
		(*it_trackerList)->setInputCloud(cloud_pass_downsampled_);
		(*it_trackerList)->compute();
		//new_cloud_ = true;

		it_trackerList++;
	}
}


/*void mainTrackClusters(){

	map<int, vector<string>> pointCloudFiles_list;
	map<int, PointCloud<PointXYZ>::Ptr> pointCloud_list;


	string outputCloudPathsAndCalibration[3][2] = {
			{ "C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\girafa\\Output2\\sample", "girafa.ini" },
			{ "C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\silvia\\Output2\\sample", "silvia.ini" },
			{ "C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\surface\\Output2\\sample", "surface.ini" }
	}; // filepath, calibration filename

	//int rows = sizeof(outputCloudPathsAndCalibration) / sizeof(outputCloudPathsAndCalibration[0]);
	//int columns = sizeof(outputCloudPathsAndCalibration) / sizeof(outputCloudPathsAndCalibration[1]);

	//cout << rows << endl;
	//cout << columns << endl;

	for (int i = 0; i < 3; i++){
		vector<string> pointCloudFilenames = getFilesInDirectory(outputCloudPathsAndCalibration[i][0]);
		std::cout << i << " :: " << outputCloudPathsAndCalibration[i][0] << std::endl;
		cout << "size = " << pointCloudFilenames.size() << endl;
		pointCloudFiles_list.insert(make_pair(i, pointCloudFilenames));
	}


	map<int, vector<string>>::iterator it_pointCloudFiles = pointCloudFiles_list.begin();
	map<int, PointCloud<PointXYZ>::Ptr>::iterator it_pointCloud;
	int pointCloudFilesIndex = 0;
	while (it_pointCloudFiles != pointCloudFiles_list.end()) {
		int file_index = 0;
		vector<string>::const_iterator iter;
		for (iter = it_pointCloudFiles->second.begin(); iter != it_pointCloudFiles->second.end(); ++iter)
		{
			PointCloud<PointXYZRGB>::Ptr cloud(new PointCloud<PointXYZRGB>);
			PointCloud<PointXYZ>::Ptr cloudXYZ(new PointCloud<PointXYZ>);;

			loadPointCloudNoFormat(cloud, *iter);
			copyPointCloud<PointXYZRGB, PointXYZ>(*cloud, *cloudXYZ);

			cout << "file " << *iter << " - calibration file = " << outputCloudPathsAndCalibration[pointCloudFilesIndex][1] << endl;
			// TODO: check if is better to apply calibration after concatenation
			cloudXYZ = ApplyCalibrationToPointCloud(cloudXYZ, outputCloudPathsAndCalibration[pointCloudFilesIndex][1]); 

			it_pointCloud = pointCloud_list.find(file_index);
			if (it_pointCloud != pointCloud_list.end()) {
				//concatenate point clouds
				cout << "concatenate point clouds - index: " << file_index << endl;
				PointCloud<PointXYZ>::Ptr cloudtemp = pointCloud_list.find(file_index)->second;
				*cloudtemp += *cloudXYZ;
				pointCloud_list[file_index] = cloudtemp;
				//cout << "point cloud size = " << pointCloud_list[file_index]->points.size() << endl;
			}
			else {
				// name pair
				cout << "insert point clouds" << endl;
				pointCloud_list.insert(make_pair(file_index, cloudXYZ));
			}
			file_index++;
		}
		pointCloudFilesIndex++;
		it_pointCloudFiles++;

		//std::cout << it->first << " :: " << it->second << std::endl;
	}


	bool trackingModelSet = false;
	it_pointCloud = pointCloud_list.begin();
	while (it_pointCloud != pointCloud_list.end()) {

		//cloud_cb(it_pointCloud->second);
		// EuclideanClusterExtraction
		if (!trackingModelSet) {
			int j = 0;
			std::vector<pcl::PointIndices> cluster_indices = EuclideanClusterExtractionSegmentation(it_pointCloud->second);
			for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); ++it)
			{
				pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster(new pcl::PointCloud<pcl::PointXYZ>);
				for (std::vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); ++pit)
					cloud_cluster->points.push_back((it_pointCloud->second)->points[*pit]); //*
				cloud_cluster->width = cloud_cluster->points.size();
				cloud_cluster->height = 1;
				cloud_cluster->is_dense = true;

				setupModelTracking(cloud_cluster);
				cout << "setting cluster " << j << endl;
				j++;

			}
			trackingModelSet = true;
			//visualizePointCloudClusters(cluster_indices, it_pointCloud->second);
			//cluster_indices.clear();
		}
		else {
			cloud_cb(it_pointCloud->second);
		}
		drawTrackingResults();
		it_pointCloud++;
	}

}*/

// --------------
// -----Main-----
// --------------
int
main(int argc, char** argv)
{
	// map allocator for the point clouds filenames 
	vector<string> pointCloudFilesGirafa;
	vector<string> pointCloudFilesSilvia;
	vector<string> pointCloudFilesSurface;
	list<OutputCloud*> outputCloudList;

	// array for the directories where the point clouds are stored
		string outputCloudPathsAndCalibration[3][2] = {
				{ "C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\girafa\\Output2\\sample", "girafa.ini" },
				{ "C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\silvia\\Output2\\sample", "silvia.ini" },
				{ "C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\surface\\Output2\\sample", "surface.ini" }
	}; // filepath, calibration filename

	//load point cloud filenames into to the appropriate lists
	pointCloudFilesGirafa = getFilesInDirectory(outputCloudPathsAndCalibration[0][0]);
	pointCloudFilesSilvia = getFilesInDirectory(outputCloudPathsAndCalibration[1][0]);
	pointCloudFilesSurface = getFilesInDirectory(outputCloudPathsAndCalibration[2][0]);
	
	if (pointCloudFilesGirafa.size() != pointCloudFilesSilvia.size()
		&& pointCloudFilesSilvia.size() != pointCloudFilesSurface.size()
		&& pointCloudFilesGirafa.size() != pointCloudFilesSurface.size()) {

		cout << "ERROR: number of point clouds for each viewpoint doesnt match!" << endl;
		return -1;
	}

	int numberOfPointCloudFiles = pointCloudFilesSilvia.size();
	for (int i = 0; i < numberOfPointCloudFiles; i++){
		OutputCloud *outputCloud = new OutputCloud();
		map<string, string> filenameByCalibrationpath;
		
		//girafa
		filenameByCalibrationpath.insert(make_pair(pointCloudFilesGirafa[i], outputCloudPathsAndCalibration[0][1]));
		cout << "filename = " << pointCloudFilesGirafa[i] << " calibration file = " << outputCloudPathsAndCalibration[0][1] << endl;
		// silvia
		filenameByCalibrationpath.insert(make_pair(pointCloudFilesSilvia[i], outputCloudPathsAndCalibration[1][1]));
		cout << "filename = " << pointCloudFilesSilvia[i] << " calibration file = " << outputCloudPathsAndCalibration[1][1] << endl;
		// surface
		filenameByCalibrationpath.insert(make_pair(pointCloudFilesSurface[i], outputCloudPathsAndCalibration[2][1]));
		cout << "filename = " << pointCloudFilesSurface[i] << " calibration file = " << outputCloudPathsAndCalibration[2][1] << endl;

		outputCloud->loadPointClouds(filenameByCalibrationpath);
		filenameByCalibrationpath.clear();
		//outputCloudList*/

		cout << "point cloud number of points = " << outputCloud->getPointCloudRGB()->points.size() << endl;
		visualizePointCloud(outputCloud->getPointCloudRGB(), 0);
		
	}


	


}

/*
int
main(int argc, char** argv)
{
	// map allocator for the point clouds filenames 
	map<int, vector<string>> pointCloudFiles_list;
	// map allocator for the point clouds loaded from the previous filenames
	map<int, PointCloud<PointXYZRGB>::Ptr> pointCloud_list;

	// array for the directories where the point clouds are stored
	string outputCloudPathsAndCalibration[3][2] = { 
	{ "C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\girafa\\Output2\\sample", "girafa.ini" },
	{"C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\silvia\\Output2\\sample", "silvia.ini" },
	{"C:\\Users\\Public\\Data\\JoaoFiadeiro\\SecondSession\\SecondTestPointClouds\\surface\\Output2\\sample", "surface.ini"} 
	}; // filepath, calibration filename

	//load point cloud filenames into pointCloudFiles_list
	for (int i = 0; i < 3; i++){
		vector<string> pointCloudFilenames = getFilesInDirectory(outputCloudPathsAndCalibration[i][0]);
		std::cout << i << " :: " << outputCloudPathsAndCalibration[i][0] << std::endl;
		cout << "size = " << pointCloudFilenames.size() << endl;
		pointCloudFiles_list.insert(make_pair(i, pointCloudFilenames));
	}

	// iterate over each point cloud filename and each viewpoint, apply calibration, concatena point clouds. 
	map<int, vector<string>>::iterator it_pointCloudFiles;// = pointCloudFiles_list.begin();
	map<int, PointCloud<PointXYZRGB>::Ptr>::iterator it_pointCloud;
	int pointCloudFilesIndex = 0;
	//while (it_pointCloudFiles != pointCloudFiles_list.end()) {
	for (it_pointCloudFiles = pointCloudFiles_list.begin(); it_pointCloudFiles != pointCloudFiles_list.end(); it_pointCloudFiles++){
		vector<string>::const_iterator iter;
		for (iter = it_pointCloudFiles->second.begin(); iter != it_pointCloudFiles->second.end(); ++iter)
		{
			PointCloud<PointXYZRGB>::Ptr cloud(new PointCloud<PointXYZRGB>);
			loadPointCloudNoFormat(cloud, *iter);
			
			cout << "file " << *iter << " - calibration file = " << outputCloudPathsAndCalibration[pointCloudFilesIndex][1] << endl;
			
			// apply calibration
			cloud = ApplyCalibrationToPointCloud(cloud, outputCloudPathsAndCalibration[pointCloudFilesIndex][1]);
			//visualizePointCloud(cloud, 0);

			// extract cloud number
			string perfix = "outputCloud";
			int indexSeparator = (*iter).find_last_of('\\');
			string path = (*iter).substr(0, indexSeparator);

			string name = (*iter).substr(indexSeparator + 1, (*iter).length());

			int cloudNumber = atoi(name.substr(perfix.length(), name.length()).c_str());
			
			// insert and or concatenate point cloud
			it_pointCloud = pointCloud_list.find(cloudNumber);
			if (it_pointCloud != pointCloud_list.end()) {
				//concatenate point clouds
				PointCloud<PointXYZRGB>::Ptr cloudtemp = pointCloud_list.find(cloudNumber)->second;
				*cloudtemp += *cloud;
				pointCloud_list[cloudNumber] = cloudtemp;
				
				//cout << "point cloud size = " << pointCloud_list[file_index]->points.size() << endl;
			}
			else {
				// name pair
				pointCloud_list.insert(make_pair(cloudNumber, cloud));
			}

			

		}
		pointCloudFilesIndex++;
		//it_pointCloudFiles++;
	
		//std::cout << it->first << " :: " << it->second << std::endl;
	}

	
	
	// map allocator for the point clouds loaded from the previous filenames
	map<int, PointCloud<PointXYZ>::Ptr>cluster_list;
	it_pointCloud = pointCloud_list.begin();
	while (it_pointCloud != pointCloud_list.end()) {


		PointCloud<PointXYZ>::Ptr cloudXYZ(new PointCloud<PointXYZ>);
		copyPointCloud<PointXYZRGB, PointXYZ>(*it_pointCloud->second, *cloudXYZ);

		// EuclideanClusterExtraction
		std::vector<pcl::PointIndices> cluster_indices = EuclideanClusterExtractionSegmentation(cloudXYZ);
		visualizePointCloudClusters(cluster_indices, cloudXYZ);
		cluster_indices.clear();

		for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); ++it)
		{
			pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster(new pcl::PointCloud<pcl::PointXYZ>);
			for (std::vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); ++pit)
				cloud_cluster->points.push_back(cloudXYZ->points[*pit]); //*
			cloud_cluster->width = cloud_cluster->points.size();
			cloud_cluster->height = 1;
			cloud_cluster->is_dense = true;

			// compute cluster centroid coordinates
			Eigen::Vector4f centroid;
			pcl::compute3DCentroid(*cloud_cluster, centroid);

		}

		it_pointCloud++;
	}
	
	cout << "FINISHED SEGMENTING POINT CLOUD DATA" << endl;

	getchar();
	return 0;

	// region growing segmentation
	//RegionGrowingSegmentation();

	
	}*/