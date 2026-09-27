import { act, useReducer } from "react"


interface UIFile {
    full_path: string[]
    file_name: string
    mimetype: string
    isDir: boolean
    children: { [name: string]: UIFile }
    mod_time: number
};

interface FileAction {
    path: string[]
    action: "FETCH" | "ADD" | "DELETE"
    files?: UIFile[]
    filename?: string;
};

function DirectoryManager({ username }: { username: string }) {

    let [files, setFiles] = useReducer((prev: UIFile, action: FileAction) => {
        prev = { ...prev }
        let currentDir = prev
        for (let dir of action.path) {
            let nextDir: UIFile = currentDir.children[dir]
            if (nextDir) {
                console.log("Path", action.path, "Not found")
                return prev
            }
        }
        switch (action.action) {
            case "ADD":
                action.files?.forEach((file) => {
                    file.full_path = [...action.path]
                    currentDir.children[file.file_name] = file
                })
                break
            case "DELETE":
                action.filename && delete currentDir.children[action.filename]
                break;
            default:
                break;
        }
        return prev
    }, {
        children: {},
        file_name: "webroot",
        full_path: [],
        isDir: true,
        mimetype: "directory",
        mod_time: new Date().valueOf()
    })



    return <p>{username}</p>
}
export default DirectoryManager
