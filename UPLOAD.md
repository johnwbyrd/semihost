# Uploading Pages to MediaWiki

## Initial Batch (9 pages)

The first batch of pages is ready to upload:

**Templates (4 pages)**:
- Template:Warning
- Template:Note
- Template:SeeAlso
- Template:ZBC Navigation

**Content Pages (5 pages)**:
- Main Page
- What is Zero Board Computer?
- Getting Started
- System Overview
- Semihosting Overview

## Method 1: Manual Upload (Recommended for First Batch)

1. Navigate to https://www.zeroboardcomputer.com
2. Log in as admin
3. For each page:
   - Search for the page name (e.g., "Main Page")
   - Click "Create this page"
   - Copy content from corresponding .wiki file
   - Paste into editor
   - Click "Save"

### Templates First!

Upload templates **before** content pages:
1. Template:Warning
2. Template:Note
3. Template:SeeAlso
4. Template:ZBC Navigation

Then upload content pages:
1. Main Page
2. What is Zero Board Computer?
3. Getting Started
4. System Overview
5. Semihosting Overview

## Method 2: Automated Upload Script

### Setup

1. **Copy config file**:
   ```bash
   cp config.ini.example config.ini
   ```

2. **Edit config.ini** with your credentials:
   ```ini
   [wiki]
   url = https://www.zeroboardcomputer.com/api.php
   username = your_username
   password = your_password
   ```

3. **Optional: Create bot password** (more secure):
   - Navigate to: https://www.zeroboardcomputer.com/wiki/Special:BotPasswords
   - Create new bot password
   - Use bot username and password in config.ini

### Usage

**Dry run (see what would be uploaded)**:
```bash
./upload_wiki.py --config config.ini --dry-run
```

**Interactive upload (with confirmation)**:
```bash
./upload_wiki.py --config config.ini
```

**Batch upload (no confirmation)**:
```bash
./upload_wiki.py --config config.ini --batch
```

**Upload without config file**:
```bash
./upload_wiki.py --username admin --password yourpass
```

## After Uploading First Batch

1. **Verify templates work**: Check that {{Warning|...}} displays correctly
2. **Verify navigation**: Check that {{ZBC Navigation}} shows all sections
3. **Check cross-links**: Verify [[Page Name]] links work
4. **Test on mobile**: Ensure formatting looks good

## Generating Remaining Pages

Once the first batch looks good:

1. Generate remaining ~40 pages
2. Use upload script for bulk upload
3. Review and fix any issues
4. Continue iterating

## Troubleshooting

**Login fails**:
- Check username/password
- Try creating bot password
- Verify API is enabled: Special:Version → API

**Upload fails**:
- Check permissions (need edit rights)
- Verify page doesn't already exist (script won't overwrite by default)
- Check MediaWiki logs

**Templates don't render**:
- Ensure template pages uploaded first
- Template names must match exactly
- Check template syntax

## Security Notes

- **Never commit config.ini** (already in .gitignore)
- Use bot passwords instead of main password
- Limit bot password permissions
- Revoke bot password when done with bulk upload
