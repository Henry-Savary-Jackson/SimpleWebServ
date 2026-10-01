import { useContext, useState } from "react"
import { listDirectory, type FileData } from "../../utils/RequestUtils"
import FileTypeIcon from "./FileTypeIcon"
import { FileTreeContext } from "../../providers/FileTreeProvider"
import { Button, Collapse, Stack } from "react-bootstrap"
import RemoveFileButton from "./RemoveFileButton"
import styles from "../css/Directory.module.css"
import AddFileButton from "./AddFileButton"

export interface UIFile extends FileData {
    full_path: string
    children: string[]
};

export default function FileElement({ path }: { path: string }) {

    const { file_tree, addFiles } = useContext(FileTreeContext)
    const file: UIFile = file_tree[path]

    let [childrenShow, setChildrenShow] = useState<boolean>(false)

    return <Stack>
        <FileTypeIcon mimetype={file.mimetype} />
        <span>{file.file_name}</span>
        <span>{new Date(file.mod_time).toLocaleTimeString()}</span>
        <Button onClick={async (e) => {
            setChildrenShow(!childrenShow)
            !childrenShow && addFiles(path, await listDirectory(path, file.file_name))
        }}>Show children</Button>
        <RemoveFileButton filePath={path} />
        {file.children &&
            <Collapse in={childrenShow}>
                <Stack>
                    {file.children.map((child) => <FileElement path={`${path}/${child}`} />)}
                </Stack>
            </Collapse>}
        {file.isDir && <AddFileButton path={path} />}

    </Stack>
}
