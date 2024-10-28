English | [简体中文](README-zh.md)

## Description

This module works with [nginx-upload-module](https://github.com/fdintino/nginx-upload-module) to rename files uploaded to the directory specified by `upload_store`

for simple renaming of file upload services, without starting other backend services to make file changes

The article on simple file uploads with the nginx-upload-module can be found [here](https://blog.noicdi.com/posts/6159a89c)(Chinese articles)

## Directives

### upload_files_rename

- Syntax：`upload_files_rename on`
- Default：—
- Context：`location`

> Rename the uploaded file, set according to the file name before uploading. Moving files to other specified directories is not supported at the moment

## Example configuration

Copy `ngx_http_upload_rename/uploadSuccess/` to the `html` directory of Nginx, which will return the page in the directory after a successful upload

```nginx
    server {
        listen 80;

        client_max_body_size 100m;
        
        # Upload form should be submitted to this location
        location /upload {
            # Pass altered request body to this location
            upload_pass /uploadSuccess;

            # Store files to this directory
            # The directory is hashed, subdirectories 0 1 2 3 4 5 6 7 8 9 should exist
            upload_store /tmp;

            # Allow uploaded files to be read only by user
            upload_store_access user:rw group:rw all:rw;

            # Set specified fields in request body
            upload_set_form_field $upload_field_name.name "$upload_file_name";
            upload_set_form_field $upload_field_name.content_type "$upload_content_type";
            upload_set_form_field $upload_field_name.path "$upload_tmp_path";

            # Inform backend about hash and size of a file
            upload_aggregate_form_field $upload_field_name.md5 "$upload_file_md5";
            upload_aggregate_form_field $upload_field_name.size "$upload_file_size";

            upload_pass_form_field "^submit$|^description$";

            upload_cleanup 400 404 499 500-505;
        }

        # Pass altered request body to a backend
        location /uploadSuccess {
            upload_files_rename on;
        }
    }
```

## Tips

This module is only suitable for simple file upload service

For file A and B with the same file name but different contents, after uploading file A first and file B again, file A will be deleted and file B will be kept, i.e. there is no setting for collision processing of files with the same name, please be aware of this.

The `location/uploadSuccess` specified by `upload_pass` may not be able to start other module functions, but only this module (not tested yet) due to the choice of mounting method during module development
