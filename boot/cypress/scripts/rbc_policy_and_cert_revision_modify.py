#!/usr/bin/env python

import json

with open("./policy/policy_reprovisioning_secure.json", "r+") as f:
    data = json.load(f)
    data["device_policy"]["flow_control"]["sys_reset_req"]["value"] = True
    f.seek(0)
    json.dump(data, f)
    f.truncate()
    f.close()

with open("./packets/debug_cert.json", "r+") as f:
    data = json.load(f)
    data["device_id"]["revision_id"] = "0x00"
    f.seek(0)
    json.dump(data, f)
    f.truncate()
    f.close()
