import { createContext, useReducer } from "react";
import type { UIFile } from "../components/directory_manager/FileElement";
import type { FileData } from "../utils/RequestUtils";

interface FileTreeAPI {

    file_tree: { [path: string]: UIFile }
    deleteFile: (path: string) => void
    addFile: (path: string, file: FileData) => void
    addFiles: (path: string, files: FileData[]) => void
}

export var FileTreeContext = createContext<FileTreeAPI>({
    file_tree: {},
    deleteFile: (path: string) => { },
    addFile: (path: string, file: FileData) => { },
    addFiles: (path: string, files: FileData[]) => { }
});

interface FileAction {
    path: string
    action: "FETCH" | "ADD" | "DELETE"
    files?: UIFile[]
    filename?: string;
};

export function FileTreeProvider({ children }) {
    let [file_tree, dispatchFileTree] = useReducer((prev: { [path: string]: UIFile }, action: FileAction) => {
        prev = { ...prev } // make a copy to get a new object
        switch (action.action) {
            case "ADD":

                action.files?.forEach((file) => {
                    // add to dict
                    prev[`${action.path}/${file.file_name}`] = { ...file }
                    let prevParent = prev[action.path]
                    // update parent with new children
                    prev[action.path] = { ...prevParent, children: [...prevParent.children, file.file_name] }
                })
                break
            case "DELETE":
                let file = prev[action.path]
                // delete file from dict
                delete prev[action.path]
                // update parent to no longer list child
                let parent = prev[file.full_path]
                prev[file.full_path] = { ...parent, children: parent.children.filter((name) => name === file.file_name) }
                // delete children if direcotry
                file.children.forEach((child) => {
                    delete prev[`${file.full_path}/${file.file_name}/${child}`]
                })
                break;
            default:
                break;
        }
        return prev
    }, {})


    const deleteFile = (path: string) => {
        dispatchFileTree({ action: "DELETE", path: path })
    }
    const addFiles = (path: string, files: FileData[]) => {
        dispatchFileTree({
            action: "ADD", path: path, files: files.map((f) => {
                return { ...f, children: [], full_path: path }
            }
            )
        })
    }

    const addFile = (path: string, file: FileData) => { addFiles(path, [file]) }


    return <FileTreeContext.Provider value={{ addFiles, addFile, file_tree, deleteFile }}>
        {children}
    </FileTreeContext.Provider>



}
