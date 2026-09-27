import { act, useReducer } from "react"
import type { UIFile } from "../directory_manager/FileElement";
import { Prev } from "react-bootstrap/esm/PageItem";

interface FileAction {
    path: string
    action: "FETCH" | "ADD" | "DELETE"
    files?: UIFile[]
    filename?: string;
};

function DirectoryManager({ username }: { username: string }) {

    let [files, setFiles] = useReducer((prev: {[path:string]:UIFile}, action: FileAction) => {
        prev = { ...prev } // make a copy to get a new object
        switch (action.action) {
            case "ADD":

                action.files?.forEach((file) => {
                    // add to dict
                    prev[`${action.path}/${file.file_name}`] = {...file}
                    let prevParent = prev[action.path]
                    // update parent with new children
                    prev[action.path] = {...prevParent, children:[...prevParent.children, file.file_name]}
                })
                break
            case "DELETE":
                let file =  prev[action.path]
                // delete file from dict
                delete prev[action.path]
                // update parent to no longer list child
                let parent = prev[file.full_path]
                prev[file.full_path] = {...parent, children:parent.children.filter((name)=>name===file.file_name)}
                // delete children if direcotry
                file.children.forEach((child)=>{
                    delete prev[`${file.full_path}/${file.file_name}/${child}`]
                })
                break;
            default:
                break;
        }
        return prev
    }, {})



    return <p>{username}</p>
}
export default DirectoryManager
