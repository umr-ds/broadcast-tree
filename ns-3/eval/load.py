import pandas
import os
import glob
import re
import io

csv_header = [
    "Total Energy",
    "Total TX Power",
    "Time to Build",
    "Time to Build (last node)",
    "Tree Depth",
    "Unconnected Nodes",
    "Cycles",
    "Cycles Lasted",
    "Energy CycleCheck",
    "Energy NeightbourDiscovery",
    "Energy ChildRequest",
    "Energy ChildConfirmation",
    "Energy ChildRejection",
    "Energy ParentRevocation",
    "Energy EndOfGame",
    "Energy ApplicationData",
    "Data CycleCheck",
    "Data NeightbourDiscovery",
    "Data ChildRequest",
    "Data ChildConfirmation",
    "Data ChildRejection",
    "Data ParentRevocation",
    "Data EndOfGame",
    "Data ApplicationData",
    "Packets CycleCheck",
    "Packets NeightbourDiscovery",
    "Packets ChildRequest",
    "Packets ChildConfirmation",
    "Packets ChildRejection",
    "Packets ParentRevocation",
    "Packets EndOfGame",
    "Packets ApplicationData",
] + [f"PacketLoss Depth {i}" for i in range(32)]

code_header = [
    "Total Energy",
    "Total Construction Energy",
    "Total Application Energy",
    "Total TX Power",
    "Time to Build",
    "Time to Build (last node)",
    "Tree Depth",
    "Unconnected Nodes",
    "Cycles",
    "Cycles Lasted",
    "Energy CycleCheck Recv",
    "Energy NeightbourDiscovery Recv",
    "Energy ChildRequest Recv",
    "Energy ChildConfirmation Recv",
    "Energy ChildRejection Recv",
    "Energy ParentRevocation Recv",
    "Energy EndOfGame Recv",
    "Energy ApplicationData Recv",
    "Energy CycleCheck Sent",
    "Energy NeightbourDiscovery Sent",
    "Energy ChildRequest Sent",
    "Energy ChildConfirmation Sent",
    "Energy ChildRejection Sent",
    "Energy ParentRevocation Sent",
    "Energy EndOfGame Sent",
    "Energy ApplicationData Sent",
    "Data CycleCheck Recv",
    "Data NeightbourDiscovery Recv",
    "Data ChildRequest Recv",
    "Data ChildConfirmation Recv",
    "Data ChildRejection Recv",
    "Data ParentRevocation Recv",
    "Data EndOfGame Recv",
    "Data ApplicationData Recv",
    "Data CycleCheck Sent",
    "Data NeightbourDiscovery Sent",
    "Data ChildRequest Sent",
    "Data ChildConfirmation Sent",
    "Data ChildRejection Sent",
    "Data ParentRevocation Sent",
    "Data EndOfGame Sent",
    "Data ApplicationData Sent",
    "Packets CycleCheck Recv",
    "Packets NeightbourDiscovery Recv",
    "Packets ChildRequest Recv",
    "Packets ChildConfirmation Recv",
    "Packets ChildRejection Recv",
    "Packets ParentRevocation Recv",
    "Packets EndOfGame Recv",
    "Packets ApplicationData Recv",
    "Packets CycleCheck Sent",
    "Packets NeightbourDiscovery Sent",
    "Packets ChildRequest Sent",
    "Packets ChildConfirmation Sent",
    "Packets ChildRejection Sent",
    "Packets ParentRevocation Sent",
    "Packets EndOfGame Sent",
    "Packets ApplicationData Sent",
] + [f"PacketLoss Depth {i}" for i in range(32)]

filename_header = [
    "Node Count",
    "Hops",
    "BTP",
    "Width",
    "Heigth",
    "Seed",
    "Cycle Prevention",
    "RTS/CTS",
    "Energy Model",
]

def load_csv(path, config=None, header=csv_header):
    if not config:
        config = os.path.split(path)[-1].split(".")[0].split("_")

    df = pandas.read_csv(path, names=header, delimiter=';', decimal="." if header != csv_header else ",")
    for k,v in zip(filename_header, config):
        try:
            v = int(v)
        except ValueError:
            v = True if v == "true" else v
            v = False if v == "false" else v
        df[k] = v

    # set Run and Seed field to usable values
    try:
        df["Run"] = df.index
        df["Seed"] += df.index
        df["Time to Build (ms)"] = df["Time to Build"] / 1000000
    except Exception as e:
        print(path)
        display(df)
        raise e

    # see EEPTProtocolHelper.h
    df.loc[df["Cycle Prevention"] == 0, "Cycle Prevention"] = "Ping to Source"
    df.loc[df["Cycle Prevention"] == 1, "Cycle Prevention"] = "Mutex"
    df.loc[df["Cycle Prevention"] == 2, "Cycle Prevention"] = "Path to Source"

    df.loc[df["RTS/CTS"] == True, "RTS/CTS"] = "RTS/CTS"
    df.loc[df["RTS/CTS"] == False, "RTS/CTS"] = ""

    df["Mode"] = df["Cycle Prevention"] + df["RTS/CTS"]

    df['Unconnected Nodes'] = pandas.to_numeric(df['Unconnected Nodes'])
    df['Node Count'] = pandas.to_numeric(df['Node Count'])
    df['Cycles Lasted'] = pandas.to_numeric(df['Cycles Lasted'])
    df["Tree Depth"] = pandas.to_numeric(df["Tree Depth"])

    if "Energy ApplicationData" in df:
        df["Energy Overhead"] = df["Total Energy"] - df["Energy ApplicationData"]

    return df

def load_data(path):
    config = os.path.split(path)[-1].split(".")[0].split("_")

    with open(path, 'r') as log_file:
        code_lines = '\n'.join([line[6:] for line in log_file.readlines() if line.startswith("CODE:")])
        df = load_csv(io.StringIO(code_lines), config, header=code_header)

        frame_types = [
            "CycleCheck",
            "NeightbourDiscovery",
            "ChildRequest",
            "ChildConfirmation",
            "ChildRejection",
            "ParentRevocation",
            "EndOfGame",
            "ApplicationData"
        ]

        for col_type in ["Energy", "Data", "Packets"]:
            for frame_type in frame_types:
                df[f"{col_type} {frame_type}"] = df[f"{col_type} {frame_type} Sent"] + df[f"{col_type} {frame_type} Recv"]

        df["Energy Overhead"] = df["Total Energy"] - df["Energy ApplicationData"]

        return df

if __name__ == "__main__":
    df = load_data("../results/2021-11-11_125435/10_3_true_500_500_6900_1_true.log")
    display(df)