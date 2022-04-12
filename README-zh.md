[English](README.md) | 简体中文

## Description

此模块与 [nginx-upload-module](https://github.com/fdintino/nginx-upload-module) 配合，将其上传到`upload_store`指定目录下的文件重命名

适用简单的文件上传服务的重命名，无需启动其他后端服务做文件改动

配合 nginx-upload-module 实现简单文件上传的相关文章可见 [此处](https://www.xqmq.icu/posts/6159a89c.html)

## Directives

### upload_files_rename

- Syntax：`upload_files_rename on`
- Default：—
- Context：`location`

> 将上传文件重命名，根据上传前的文件名设定。暂不支持移动文件到其他指定目录

## Example configuration

复制`ngx_http_upload_rename/uploadSuccess/`到 Nginx 的`html`目录下，上传成功后将返回目录中的页面

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

本模块只适合简单的文件上传服务

对于同文件名但不同内容的文件 A 和文件 B，先上传文件 A，再次上传文件 B 后，会删除 文件 A 保留文件 B，即没有设置同名文件碰撞处理，请知悉

由于模块开发过程中挂载方式的选择，`upload_pass`指定的`location/uploadSuccess`可能无法启动其他模块的功能，只能启动本模块（暂未测试）



