import { useContext, useState } from "react"
import { FileTreeContext } from "../../providers/FileTreeProvider"
import { Alert, Button, Form, FormControl, FormGroup, FormLabel, Modal, ModalBody, ModalDialog, ModalFooter, ModalHeader } from "react-bootstrap"
import { postDirectory, postFile, type FileData } from "../../utils/RequestUtils"
import { CSRFContext } from "../../providers/CSRFProvider"
import { FileModalContext } from "../../providers/AddFileModalProvider"


export default function addFileForm({ filePath, show}: { filePath: string, show: boolean}) {
    const { addFile } = useContext(FileTreeContext)
    const {hideModal} = useContext(FileModalContext)
    const csrf = useContext(CSRFContext)

    let [file, setFile] = useState(null)
    let [fileName, setFileName] = useState("")
    let [isDir, setIsDir] = useState(false)

    let [error, setError] = useState("")

    return <Modal show={show}><ModalDialog >
        <ModalHeader>
            {error && <Alert variant="danger">{error}</Alert>}
        </ModalHeader>
        <ModalBody>
            <Form>
                <FormGroup>
                    <FormLabel htmlFor="fileInp"></FormLabel>
                    <FormControl id="fileName" type="text" onChange={(e) => {
                        e.stopPropagation()
                        setFileName(e.target.value)
                    }} value={fileName} />
                </FormGroup>
                <FormGroup>
                    <FormLabel htmlFor="fileInp"></FormLabel>
                    <FormControl id="fileInp" type="file" onChange={(e) => {
                        e.stopPropagation()
                        setFile((e.currentTarget as HTMLInputElement).files[0])
                    }} />
                </FormGroup>
                <FormGroup>
                    <FormLabel htmlFor="isDirInp"></FormLabel>
                    <FormControl id="isDirInp" type="checkbox" onChange={(e) => {
                        e.stopPropagation()
                        setIsDir((e.currentTarget as HTMLInputElement).checked)
                    }} checked={isDir} />
                </FormGroup>
            </Form>
        </ModalBody>
        <ModalFooter>
            <Button type="button" onClick={async (e) => {
                e.stopPropagation()
                if (!fileName) {
                    setError("Please Enter a filename!")
                    return
                }
                if (!file && !isDir) {
                    setError("Please Enter a file!")
                    return

                }

                if (isDir){
                    const newFile =await postDirectory(filePath,fileName, csrf)
                    addFile(filePath ,newFile)
                    return
                }
                const reader = new FileReader()
                reader.onload = async (e) => {
                    const data = e.target.result as ArrayBuffer
                    const newFile : FileData = await postFile(filePath, fileName, data, csrf)
                    addFile(filePath,newFile)

                };
                reader.readAsArrayBuffer(file)

            }} variant="success">Add File</Button>
            <Button type="button" onClick={(e) => {
                e.stopPropagation()
                hideModal()
            }} variant="success">Cancel</Button>
        </ModalFooter>
    </ModalDialog></Modal>

}
