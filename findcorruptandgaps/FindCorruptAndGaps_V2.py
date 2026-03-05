import pandas as pd

file_no = "8823"
timestamp_col = "Date & Time"
gap_threshold_seconds = 0.5

# load file
expected_header = ["Date & Time,PPGVal,Xval,Yval,Zval"]
headers = []
data = {}

with open(f"test.csv") as f:
    for c, line in enumerate(f):
        line = line.strip("\n")

        if c == 0:
            headers = line.split(",")
            data = {h: [] for h in headers}
            expected_header = line
        else:
            if line == expected_header:
                continue
            values = line.split(",")
            for cv, v in enumerate(values):
                data[headers[cv]].append(v)

df2 = pd.DataFrame(data)

# parse timestamps
df2["parsed_time"] = pd.to_datetime(df2["Date & Time"], errors="coerce")
df2["is_corrupt"] = df2["parsed_time"].isna()

# normal gap detection
df2["next_timestamp"] = df2["parsed_time"].shift(-1)
df2["duration"] = df2["next_timestamp"] - df2["parsed_time"]

valid_gaps = df2[df2["duration"].dt.total_seconds() > 1]

normal_gap_records = []
for i, row in valid_gaps.iterrows():

    duration_str = str(row["duration"]).split(".")[0]  # Format hh:mm:ss
    
    normal_gap_records.append({
        "type": "normal_gap",
        "start_time": row["parsed_time"],
        "end_time": row["next_timestamp"],
        "duration": duration_str,
        "corrupt_value": None
    })

# corrupt timestamp detection
corrupt_records = []
in_block = False

for i in range(len(df2)):

    if df2.loc[i, "is_corrupt"] and not in_block:
        block_start = i
        in_block = True

    if in_block and (not df2.loc[i, "is_corrupt"]):
        block_end = i - 1
        in_block = False

        prev_valid_time = df2.loc[block_start - 1, "parsed_time"] if block_start > 0 else None
        next_valid_time = df2.loc[i, "parsed_time"]

        if pd.notna(prev_valid_time) and pd.notna(next_valid_time):
            duration_td = next_valid_time - prev_valid_time
            duration_str = str(duration_td).split(".")[0]
        else:
            duration_str = None

        corrupt_records.append({
            "type": "corrupt_timestamp",
            "start_time": prev_valid_time,
            "end_time": next_valid_time,
            "duration": duration_str,
            "corrupt_value": df2.loc[block_start, "Date & Time"]  # <-- first corrupt value
        })

# handle corruption at end of file
if in_block:
    block_end = len(df2) - 1
    prev_valid_time = df2.loc[block_start - 1, "parsed_time"] if block_start > 0 else None

    corrupt_records.append({
        "type": "corrupt_timestamp",
        "start_time": prev_valid_time,
        "end_time": None,
        "duration": None,
        "corrupt_value": df2.loc[block_start, "Date & Time"]
    })

# append
all_records = normal_gap_records + corrupt_records
all_gaps_df = pd.DataFrame(all_records)

print("Checked " + str(c) + " lines..")
# Sort output chronologically
if (len(all_records) > 0):
    all_gaps_df = all_gaps_df.sort_values(by="start_time").reset_index(drop=True)

    all_gaps_df.to_csv(f"{file_no}_all_gaps_summary.csv", index=False)

    print(f"Saved unified gap summary → {file_no}_all_gaps_summary.csv")
else:
    print("No corrupt records found!")


f = open(f"{file_no}_all_gaps_summary.csv", "a")
checkRamp = True
foundError = False
if (checkRamp):
    def checkRampRecord(key):
        value = int(data[key][0])
        for i in range(1, len(data[key]) - 2):
            value += 1
            if (value >= 65536): value = 0
            if (int(data[key][i]) != value):
                print("Ramp data error at " + str(data['Date & Time'][i]))
                f.write("\nRamp data error at " + str(data['Date & Time'][i]))
                value = int(data[key][i])
                global foundError
                foundError = True

    checkRampRecord('PPGVal')
    checkRampRecord('Xval')
    checkRampRecord('Yval')
    checkRampRecord('Zval')
    if (not foundError):
        print("No errors found in ramp data!")