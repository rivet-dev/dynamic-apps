#!/bin/sh

# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
# FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
# DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
# AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
# OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# -----------------------------------------------------------------------------

# This is a reference custom action to upload screenshots to Imgur™.
# Users are encouraged to make their own copies to cover their needs
# such as authenticated upload or use different hosting services.

# Watch for sensitive content, the uploaded image will be publicly
# available and there is no guarantee it can be certainly deleted.
# Xfce is NOT affiliated with nor this script is approved by Imgur™.
# If you use this script you must agree with Imgur™ Terms of Service
# available at https://imgur.com/tos

# -----------------------------------------------------------------------------

URL='https://api.imgur.com/3/image'
SCREENSHOT_PATH=$1
CLIENT_ID=$2

if [ -z "$SCREENSHOT_PATH" ] || [ -z "$CLIENT_ID" ]; then
    zenity --error --text="Arguments are missing"
    exit 1
fi

#RESPONSE='{"data":{"id":"q9a8Oh4","title":null,"description":null,"datetime":1690124891,"type":"image\/png","animated":false,"width":217,"height":186,"size":593,"views":0,"bandwidth":0,"vote":null,"favorite":false,"nsfw":null,"section":null,"account_url":null,"account_id":0,"is_ad":false,"in_most_viral":false,"has_sound":false,"tags":[],"ad_type":0,"ad_url":"","edited":"0","in_gallery":false,"deletehash":"b0AjSDJjSU4iyhE","name":"","link":"https:\/\/i.imgur.com\/q9a8Oh4.png"},"success":true,"status":200}'
#RESPONSE='{"data":{"error":{"code":1003,"message":"File type invalid (1)","type":"ImgurException","exception":[]},"request":"\/3\/image","method":"POST"},"success":false,"status":400}'
RESPONSE=$(curl --silent --location "$URL" --header "Authorization: Client-ID $CLIENT_ID" --form "image=@$SCREENSHOT_PATH")
STATUS=$(echo "$RESPONSE" | jq -r .status)

if [ -z "$STATUS" ] || [ $STATUS -ne 200 ]; then
    ERROR=$(echo "$RESPONSE" | jq -r .data.error.message)
    zenity --error --text="Failed to upload screenshot:\n$ERROR"
    exit 1
fi

LINK="https://imgur.com/$(echo "$RESPONSE" | jq -r .data.id).png"
DELETE="https://imgur.com/delete/$(echo "$RESPONSE" | jq -r .data.deletehash)"
LOG_DIRECTORY="${XDG_DATA_HOME:-$HOME/.local/share}/xfce4"
LOG="$LOG_DIRECTORY/xfce4-screenshooter-imgur.log"

# Add link to clipboard
echo "$LINK" | xclip -selection c

# Add links to log
mkdir -p "$LOG_DIRECTORY"
echo "---
$(date '+%x %X')
Link: $LINK
Delete: $DELETE" >> "$LOG"

# Show dialog with links
zenity --info --title="Screenshot uploaded" --text="Link: <a href='$LINK'>$LINK</a>
Delete: <a href='$DELETE'>$DELETE</a>

Link copied to clipboard. Links stored in:
$LOG"
