import urllib.request
import json
import sys
import argparse

def publish(token, publish_status="draft"):
    # 1. Get user ID
    req = urllib.request.Request(
        "https://api.medium.com/v1/me",
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "Accept": "application/json"
        }
    )
    try:
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode())
            user_id = data["data"]["id"]
            username = data["data"]["username"]
            print(f"[+] Authenticated as @{username} (ID: {user_id})")
    except Exception as e:
        print(f"[-] Authentication failed: {e}")
        return

    # 2. Read Markdown content
    with open("/mnt/work/company/cyphermatrix/repos/bad-epoll-lab/article/MEDIUM_DEEP_DIVE_FINAL.md", "r") as f:
        content = f.read()

    # 3. Post story
    payload = {
        "title": "CVE-2026-46242 Deep Dive: A Working Root Exploit on Linux, 21 Dead Ends on Android",
        "contentFormat": "markdown",
        "content": content,
        "tags": ["Cybersecurity", "Linux", "Exploit Development", "Android Security", "Vulnerability Research"],
        "publishStatus": publish_status,
        "notifyFollowers": False
    }

    post_req = urllib.request.Request(
        f"https://api.medium.com/v1/users/{user_id}/posts",
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "Accept": "application/json"
        }
    )

    try:
        with urllib.request.urlopen(post_req) as resp:
            post_data = json.loads(resp.read().decode())
            url = post_data["data"]["url"]
            print(f"[+] Story successfully created!")
            print(f"[+] Status: {publish_status}")
            print(f"[+] URL: {url}")
    except Exception as e:
        print(f"[-] Failed to publish story: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Auto-publish CVE-2026-46242 deep dive to Medium")
    parser.add_argument("--token", required=True, help="Medium Integration Token (from Medium Settings -> Security and apps -> Integration tokens)")
    parser.add_argument("--public", action="store_true", help="Publish directly as public (default is draft)")
    args = parser.parse_args()

    status = "public" if args.public else "draft"
    publish(args.token, status)
