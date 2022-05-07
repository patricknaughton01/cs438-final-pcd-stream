import ast
import os
import time
from typing import Tuple
import klampt
import numpy as np
import pandas as pd
import argparse
from klampt.model import sensing
from klampt.math import se3
from klampt import vis
from tqdm import tqdm

IMG_DIR = "imgs"
LOG_FN = "state_log.txt"
CN = "realsense_overhead_5_l515"
TIME_SUFFIX = "_times.csv"


def main():
    parser = argparse.ArgumentParser(description="Extract the transforms "
        + "of the camera corresponding to the recorded images in a log")
    parser.add_argument("path", type=str, help="path to the log directory")
    parser.add_argument("robot_fn", type=str, help="path to robot file (urdf)")
    parser.add_argument("out", type=str, help="output file path")
    args = vars(parser.parse_args())
    world = klampt.WorldModel()
    robot_model: klampt.RobotModel = world.loadRobot(args["robot_fn"])
    df = pd.read_csv(os.path.join(args["path"], LOG_FN), sep="|")
    _, img_times = get_times(args["path"])
    cam_obj = robot_model.sensor(CN)
    link_ind = int(cam_obj.getSetting('link'))
    transform = sensing.get_sensor_xform(cam_obj)
    all_tfs = []
    print("Getting transforms from log")
    for t in tqdm(img_times):
        state_df = df[df["nature"]=="robot"]
        times = state_df["trina_time"]
        ind = np.searchsorted(times, t, side='right') - 1
        state = ast.literal_eval(state_df.iloc[ind]["log"])
        robot_q = state["Position"]["Robotq"]
        robot_model.setConfig(robot_q)
        c_tf = se3.mul(robot_model.link(link_ind).getTransform(), transform)
        all_tfs.append(c_tf)
    print("Writing transforms")
    with open(args["out"], "w") as f:
        for tf in tqdm(all_tfs):
            flat_tf = (*tf[0], *tf[1])
            str_tf = [str(x) for x in flat_tf]
            f.write(" ".join(str_tf) + "\n")


def get_times(log_dir: str) -> Tuple[np.ndarray, np.ndarray]:
    fn = os.path.join(log_dir, IMG_DIR, "{}{}".format(CN, TIME_SUFFIX))
    with open(fn, "r") as f:
        raw_times = f.readlines()
        # Ignore the header
        raw_times.pop(0)
        inds = []
        time_vals = []
        for line in raw_times:
            time_data = line.split("|")
            inds.append(int(time_data[0]))
            time_vals.append(float(time_data[1]))
        return (np.array(inds, dtype='int64'),
            np.array(time_vals, dtype='float64'))



if __name__ == "__main__":
    main()
