import subprocess
import re
import sys

def get_field_at_offset(struct_name, target_offset=160):
    try:
        # Run pahole to get the struct layout
        result = subprocess.run(['pahole', '-C', struct_name, 'tier2/android/artifacts/vmlinux'], 
                              capture_output=True, text=True, check=False)
        output = result.stdout
        if not output:
            return "NO_OUTPUT"
        
        # Parse the output
        for line in output.split('\n'):
            line = line.strip()
            # Match line with offset comment: /* offset size */
            # e.g., "struct list_head           urb_list;             /*    24    16 */"
            match = re.search(r'/\*\s+(\d+)\s+(\d+)\s+\*/', line)
            if match:
                offset = int(match.group(1))
                size = int(match.group(2))
                
                if offset <= target_offset < offset + size:
                    # Found the field covering our target offset
                    field_desc = line.split('/*')[0].strip()
                    return f"Offset {offset} (size {size}): {field_desc}"
                    
        return "PADDING / END_OF_STRUCT"
    except Exception as e:
        return f"ERROR: {e}"

def main():
    with open('tier2/evidence/candidate_alloc_filter.txt', 'r') as f:
        lines = f.readlines()
        
    candidates = []
    reading = False
    for line in lines:
        line = line.strip()
        if not line:
            continue
        if line.startswith("==="):
            if "Dead Ends" in line:
                break
            reading = True
            continue
        if reading:
            candidates.append(line)
            
    print(f"Checking {len(candidates)} candidates for offset 160...")
    
    for cand in candidates:
        field = get_field_at_offset(cand)
        print(f"{cand:20s}: {field}")

if __name__ == "__main__":
    main()
