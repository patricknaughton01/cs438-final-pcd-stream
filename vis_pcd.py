import open3d as o3d
import argparse

parser = argparse.ArgumentParser(description="Visualize a point cloud")
parser.add_argument("path", type=str, help="path to pcd file")
args = vars(parser.parse_args())

pcd = o3d.io.read_point_cloud(args["path"])
o3d.visualization.draw_geometries([pcd])
