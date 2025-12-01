#!/usr/bin/env python3
"""
Upload Zero Board Computer wiki pages to MediaWiki

Usage:
    ./upload_wiki.py --username USER --password PASS [--batch]
    ./upload_wiki.py --config config.ini [--batch]

Options:
    --username, -u    MediaWiki username
    --password, -p    MediaWiki password (or use bot password)
    --config, -c      Config file with credentials
    --batch, -b       Upload all pages without confirmation
    --dry-run, -d     Show what would be uploaded without uploading
"""

import requests
import argparse
import configparser
from pathlib import Path
import sys

class MediaWikiUploader:
    def __init__(self, api_url, username, password):
        self.api_url = api_url
        self.session = requests.Session()
        self.username = username
        self.password = password
        self.logged_in = False

    def login(self):
        """Login to MediaWiki"""
        # Get login token
        try:
            r = self.session.get(self.api_url, params={
                'action': 'query',
                'meta': 'tokens',
                'type': 'login',
                'format': 'json'
            })
            r.raise_for_status()
            login_token = r.json()['query']['tokens']['logintoken']
        except Exception as e:
            raise Exception(f"Failed to get login token: {e}")

        # Login
        try:
            r = self.session.post(self.api_url, data={
                'action': 'login',
                'lgname': self.username,
                'lgpassword': self.password,
                'lgtoken': login_token,
                'format': 'json'
            })
            r.raise_for_status()
            result = r.json()

            if result['login']['result'] != 'Success':
                raise Exception(f"Login failed: {result['login'].get('reason', 'Unknown reason')}")

            self.logged_in = True
            print(f"✓ Logged in as {self.username}")
        except Exception as e:
            raise Exception(f"Login failed: {e}")

    def get_csrf_token(self):
        """Get CSRF token for editing"""
        if not self.logged_in:
            self.login()

        r = self.session.get(self.api_url, params={
            'action': 'query',
            'meta': 'tokens',
            'format': 'json'
        })
        r.raise_for_status()
        return r.json()['query']['tokens']['csrftoken']

    def upload_page(self, title, content, summary=""):
        """Upload a single page"""
        if not self.logged_in:
            self.login()

        token = self.get_csrf_token()

        try:
            r = self.session.post(self.api_url, data={
                'action': 'edit',
                'title': title,
                'text': content,
                'summary': summary,
                'token': token,
                'format': 'json'
            })
            r.raise_for_status()
            result = r.json()

            if 'error' in result:
                raise Exception(result['error'].get('info', str(result['error'])))

            return result
        except Exception as e:
            raise Exception(f"Upload failed: {e}")

    def upload_directory(self, pages_dir, batch=False, dry_run=False):
        """Upload all .wiki files from directory"""
        pages_dir = Path(pages_dir).expanduser()

        if not pages_dir.exists():
            print(f"✗ Directory not found: {pages_dir}")
            return

        # Find all .wiki files
        wiki_files = sorted(pages_dir.rglob('*.wiki'))

        if not wiki_files:
            print(f"✗ No .wiki files found in {pages_dir}")
            return

        print(f"\nFound {len(wiki_files)} wiki pages to upload:")
        for wiki_file in wiki_files:
            title = self.wiki_file_to_title(wiki_file)
            print(f"  - {title}")

        if dry_run:
            print("\n[DRY RUN] No pages uploaded.")
            return

        if not batch:
            confirm = input("\nProceed with upload? [y/N] ")
            if confirm.lower() != 'y':
                print("Upload cancelled.")
                return

        print("\nUploading pages...\n")

        success_count = 0
        fail_count = 0

        for wiki_file in wiki_files:
            title = self.wiki_file_to_title(wiki_file)
            content = wiki_file.read_text(encoding='utf-8')
            summary = f"ZBC wiki page from {wiki_file.name}"

            try:
                self.upload_page(title, content, summary)
                print(f"✓ Uploaded: {title}")
                success_count += 1
            except Exception as e:
                print(f"✗ Failed: {title} - {e}")
                fail_count += 1

        print(f"\n{'='*60}")
        print(f"Upload complete: {success_count} succeeded, {fail_count} failed")

    def wiki_file_to_title(self, wiki_file):
        """Convert filename to wiki page title"""
        # e.g., "Main_Page.wiki" -> "Main Page"
        # e.g., "Template_Warning.wiki" -> "Template:Warning"

        title = wiki_file.stem

        # Handle templates
        if title.startswith('Template_'):
            title = title.replace('Template_', 'Template:', 1)

        # Replace underscores with spaces
        title = title.replace('_', ' ')

        return title

def load_config(config_file):
    """Load credentials from config file"""
    config = configparser.ConfigParser()
    config.read(config_file)

    if 'wiki' not in config:
        raise Exception("Config file must have [wiki] section")

    return {
        'url': config['wiki'].get('url', 'https://www.zeroboardcomputer.com/api.php'),
        'username': config['wiki']['username'],
        'password': config['wiki']['password']
    }

def main():
    parser = argparse.ArgumentParser(description='Upload ZBC wiki pages to MediaWiki')
    parser.add_argument('-u', '--username', help='MediaWiki username')
    parser.add_argument('-p', '--password', help='MediaWiki password')
    parser.add_argument('-c', '--config', help='Config file with credentials')
    parser.add_argument('-b', '--batch', action='store_true', help='Upload all without confirmation')
    parser.add_argument('-d', '--dry-run', action='store_true', help='Show what would be uploaded')
    parser.add_argument('--url', default='https://www.zeroboardcomputer.com/api.php', help='MediaWiki API URL')
    parser.add_argument('--pages-dir', default='~/git/zbc/pages', help='Directory containing .wiki files')

    args = parser.parse_args()

    # Get credentials
    if args.config:
        config = load_config(args.config)
        url = config['url']
        username = config['username']
        password = config['password']
    elif args.username and args.password:
        url = args.url
        username = args.username
        password = args.password
    else:
        parser.error('Must provide either --config or both --username and --password')

    # Create uploader
    uploader = MediaWikiUploader(url, username, password)

    # Upload pages
    uploader.upload_directory(args.pages_dir, batch=args.batch, dry_run=args.dry_run)

if __name__ == '__main__':
    main()
