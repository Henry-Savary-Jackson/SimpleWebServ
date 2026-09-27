import { useState } from "react"
import type { File } from "../../utils/RequestUtils"
import FileTypeIcon from "./FileTypeIcon"


export interface UIFile extends File {
    full_path: string
    children: string[]
};

export default function FileElement({file }:{file:UIFile}){

    let [childrenShow, setChildrenShow ]= useState<boolean>(false)

    return <div>
        <FileTypeIcon mimetype={file.mimetype}/>
        <span>{file.file_name}</span>
        <span>{new Date(file.mod_time).toLocaleTimeString()}</span>
    </div>
}
